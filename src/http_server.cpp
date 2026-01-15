#include "http_server.h"

#include <iostream>
#include <sstream>
#include <string>

#include "crow/app.h"

namespace ox_sim {

HttpServer::HttpServer() : simulator_(nullptr), port_(8765), running_(false), should_stop_(false) {}

HttpServer::~HttpServer() { Stop(); }

bool HttpServer::Start(SimulatorCore* simulator, int port) {
    if (!simulator) {
        return false;
    }

    if (running_.load()) {
        return false;  // Already running
    }

    simulator_ = simulator;
    port_ = port;
    should_stop_.store(false);

    server_thread_ = std::thread(&HttpServer::ServerThread, this);

    // Detach thread so it runs independently
    server_thread_.detach();

    // Give server time to start
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    return true;
}

void HttpServer::Stop() {
    if (running_.load()) {
        should_stop_.store(true);

        // NOTE: CROW doesn't have a clean shutdown mechanism in multithreaded mode
        // The server will continue running until the process exits
        // This is acceptable for a driver that runs for the lifetime of ox-service

        running_.store(false);
    }
}

void HttpServer::ServerThread() {
    std::cout << "HTTP Server starting on port " << port_ << "..." << std::endl;
    std::cout.flush();

    running_.store(true);

    crow::SimpleApp app;

    // GET /hmd/pose - Get current HMD pose
    CROW_ROUTE(app, "/hmd/pose").methods("GET"_method)([this]() {
        OxPose pose;
        simulator_->GetHMDPose(&pose);

        crow::json::wvalue response;
        response["position"]["x"] = pose.position.x;
        response["position"]["y"] = pose.position.y;
        response["position"]["z"] = pose.position.z;
        response["orientation"]["x"] = pose.orientation.x;
        response["orientation"]["y"] = pose.orientation.y;
        response["orientation"]["z"] = pose.orientation.z;
        response["orientation"]["w"] = pose.orientation.w;

        return crow::response(response);
    });

    // POST /hmd/pose - Set HMD pose
    CROW_ROUTE(app, "/hmd/pose").methods("POST"_method)([this](const crow::request& req) {
        auto json = crow::json::load(req.body);
        if (!json) {
            return crow::response(400, "Invalid JSON");
        }

        // Validate required fields exist
        if (!json.has("position") || !json.has("orientation") || !json["position"].has("x") ||
            !json["position"].has("y") || !json["position"].has("z") || !json["orientation"].has("x") ||
            !json["orientation"].has("y") || !json["orientation"].has("z") || !json["orientation"].has("w")) {
            return crow::response(400, "Missing required fields: position{x,y,z}, orientation{x,y,z,w}");
        }

        OxPose pose;
        pose.position.x = json["position"]["x"].d();
        pose.position.y = json["position"]["y"].d();
        pose.position.z = json["position"]["z"].d();
        pose.orientation.x = json["orientation"]["x"].d();
        pose.orientation.y = json["orientation"]["y"].d();
        pose.orientation.z = json["orientation"]["z"].d();
        pose.orientation.w = json["orientation"]["w"].d();

        simulator_->SetHMDPose(pose);
        return crow::response(200, "OK");
    });

    // POST /controller/<id>/pose - Set controller pose
    CROW_ROUTE(app, "/controller/<int>/pose")
        .methods("POST"_method)([this](const crow::request& req, int controller_id) {
            if (controller_id < 0 || controller_id > 1) {
                return crow::response(400, "Invalid controller ID (must be 0 or 1)");
            }

            auto json = crow::json::load(req.body);
            if (!json) {
                return crow::response(400, "Invalid JSON");
            }

            // Validate required fields
            if (!json.has("position") || !json.has("orientation") || !json["position"].has("x") ||
                !json["position"].has("y") || !json["position"].has("z") || !json["orientation"].has("x") ||
                !json["orientation"].has("y") || !json["orientation"].has("z") || !json["orientation"].has("w")) {
                return crow::response(400, "Missing required fields: position{x,y,z}, orientation{x,y,z,w}");
            }

            bool is_active = json.has("active") ? json["active"].b() : true;

            OxPose pose;
            pose.position.x = json["position"]["x"].d();
            pose.position.y = json["position"]["y"].d();
            pose.position.z = json["position"]["z"].d();
            pose.orientation.x = json["orientation"]["x"].d();
            pose.orientation.y = json["orientation"]["y"].d();
            pose.orientation.z = json["orientation"]["z"].d();
            pose.orientation.w = json["orientation"]["w"].d();

            simulator_->SetControllerPose(controller_id, pose, is_active);
            return crow::response(200, "OK");
        });

    // POST /controller/<id>/input - Set controller input state
    CROW_ROUTE(app, "/controller/<int>/input")
        .methods("POST"_method)([this](const crow::request& req, int controller_id) {
            if (controller_id < 0 || controller_id > 1) {
                return crow::response(400, "Invalid controller ID (must be 0 or 1)");
            }

            auto json = crow::json::load(req.body);
            if (!json) {
                return crow::response(400, "Invalid JSON");
            }

            // Validate required fields
            if (!json.has("component") || !json.has("value")) {
                return crow::response(400, "Missing required fields: component, value");
            }

            std::string component = json["component"].s();
            float value = 0.0f;
            bool boolean_value = false;

            // Handle both numeric and boolean values
            if (json["value"].t() == crow::json::type::Number) {
                value = json["value"].d();
            } else if (json["value"].t() == crow::json::type::True || json["value"].t() == crow::json::type::False) {
                boolean_value = json["value"].b();
                value = boolean_value ? 1.0f : 0.0f;
            }

            // Convert API component path to OpenXR format
            std::string component_path =
                "/user/hand/" + std::string(controller_id == 0 ? "left" : "right") + "/input/" + component;

            simulator_->SetInputComponent(controller_id, component_path.c_str(), value, boolean_value);
            return crow::response(200, "OK");
        });

    // Root endpoint for testing
    CROW_ROUTE(app, "/")
    ([]() {
        return "OX Simulator API Server\n\nAvailable endpoints:\n"
               "  GET  /hmd/pose\n"
               "  POST /hmd/pose\n"
               "  POST /controller/<id>/pose\n"
               "  POST /controller/<id>/input\n";
    });

    std::cout << "Starting HTTP server on port " << port_ << "..." << std::endl;
    std::cout.flush();

    try {
        app.loglevel(crow::LogLevel::Info);
        app.concurrency(1);
        app.bindaddr("127.0.0.1");
        app.port(port_);
        app.run();
    } catch (const std::exception& e) {
        std::cerr << "Exception starting server: " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "Unknown exception starting server" << std::endl;
    }

    running_.store(false);
    std::cout << "HTTP Server stopped" << std::endl;
    std::cout.flush();
}

}  // namespace ox_sim
