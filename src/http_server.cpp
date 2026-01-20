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

    // POST /device/pose - Set device pose (user_path based)
    CROW_ROUTE(app, "/device/pose").methods("POST"_method)([this](const crow::request& req) {
        auto json = crow::json::load(req.body);
        if (!json) {
            return crow::response(400, "Invalid JSON");
        }

        // Validate user_path field
        if (!json.has("user_path")) {
            return crow::response(
                400, "Missing required field: user_path (e.g., /user/head, /user/hand/left, /user/vive_tracker/waist)");
        }

        std::string user_path = json["user_path"].s();

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

        simulator_->SetDevicePose(user_path.c_str(), pose, is_active);
        return crow::response(200, "OK");
    });

    // GET /device/pose - Get device pose (user_path based)
    CROW_ROUTE(app, "/device/pose").methods("GET"_method)([this](const crow::request& req) {
        auto user_path_param = req.url_params.get("user_path");
        if (!user_path_param) {
            return crow::response(400, "Missing required query parameter: user_path");
        }

        std::string user_path = user_path_param;

        OxDeviceState devices[OX_MAX_DEVICES];
        uint32_t device_count;
        simulator_->GetAllDevices(devices, &device_count);

        for (uint32_t i = 0; i < device_count; ++i) {
            if (std::strcmp(devices[i].user_path, user_path.c_str()) == 0) {
                crow::json::wvalue response;
                response["user_path"] = devices[i].user_path;
                response["is_active"] = devices[i].is_active;
                response["position"]["x"] = devices[i].pose.position.x;
                response["position"]["y"] = devices[i].pose.position.y;
                response["position"]["z"] = devices[i].pose.position.z;
                response["orientation"]["x"] = devices[i].pose.orientation.x;
                response["orientation"]["y"] = devices[i].pose.orientation.y;
                response["orientation"]["z"] = devices[i].pose.orientation.z;
                response["orientation"]["w"] = devices[i].pose.orientation.w;
                return crow::response(response);
            }
        }

        return crow::response(404, "Device not found");
    });

    // GET /device/input - Get device input state (user_path and component_path based)
    CROW_ROUTE(app, "/device/input").methods("GET"_method)([this](const crow::request& req) {
        auto user_path_param = req.url_params.get("user_path");
        auto component_path_param = req.url_params.get("component_path");

        if (!user_path_param || !component_path_param) {
            return crow::response(400, "Missing required query parameters: user_path, component_path");
        }

        std::string user_path = user_path_param;
        std::string component_path = component_path_param;

        // Validate component path format
        if (component_path.find("/input/") != 0) {
            return crow::response(400, "Component path must start with /input/ (e.g., /input/trigger/value)");
        }

        OxInputComponentState state;
        OxComponentResult result =
            simulator_->GetInputComponentState(user_path.c_str(), component_path.c_str(), &state);

        if (result != OX_COMPONENT_AVAILABLE) {
            return crow::response(404, "Component not available");
        }

        crow::json::wvalue response;
        response["user_path"] = user_path;
        response["component_path"] = component_path;
        response["boolean_value"] = state.boolean_value;
        response["float_value"] = state.float_value;
        response["x"] = state.x;
        response["y"] = state.y;

        return crow::response(response);
    });

    // POST /device/input - Set device input state (user_path based)
    CROW_ROUTE(app, "/device/input").methods("POST"_method)([this](const crow::request& req) {
        auto json = crow::json::load(req.body);
        if (!json) {
            return crow::response(400, "Invalid JSON");
        }

        // Validate required fields
        if (!json.has("user_path") || !json.has("component_path") || !json.has("value")) {
            return crow::response(400, "Missing required fields: user_path, component_path, value");
        }

        std::string user_path = json["user_path"].s();
        std::string component_path = json["component_path"].s();
        float value = 0.0f;
        bool boolean_value = false;

        // Validate component path format
        if (component_path.find("/input/") != 0) {
            return crow::response(400, "Component path must start with /input/ (e.g., /input/trigger/value)");
        }

        // Handle both numeric and boolean values
        if (json["value"].t() == crow::json::type::Number) {
            value = json["value"].d();
            boolean_value = (value >= 0.5f);
        } else if (json["value"].t() == crow::json::type::True || json["value"].t() == crow::json::type::False) {
            boolean_value = json["value"].b();
            value = boolean_value ? 1.0f : 0.0f;
        }

        simulator_->SetInputComponent(user_path.c_str(), component_path.c_str(), value, boolean_value);
        return crow::response(200, "OK");
    });

    // Root endpoint for testing
    CROW_ROUTE(app, "/")
    ([]() {
        return "ox Simulator API Server\n\nAvailable endpoints:\n"
               "  GET  /hmd/pose              - Get HMD pose\n"
               "  POST /hmd/pose              - Set HMD pose\n"
               "  GET  /device/pose?user_path=... - Get device pose\n"
               "  POST /device/pose           - Set device pose (supports /user/head, /user/hand/left, "
               "/user/hand/right, etc.)\n"
               "  GET  /device/input?user_path=...&component_path=... - Get device input state\n"
               "  POST /device/input          - Set device input state\n";
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
