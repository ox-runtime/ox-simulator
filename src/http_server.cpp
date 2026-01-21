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

std::pair<std::string, std::string> SplitBindingPath(const std::string& binding_path) {
    size_t pos = binding_path.find("/input/");
    if (pos == std::string::npos) {
        return {"", ""};  // Invalid path
    }
    std::string user_path = binding_path.substr(0, pos);
    std::string component_path = binding_path.substr(pos);
    return {user_path, component_path};
}

void HttpServer::ServerThread() {
    std::cout << "HTTP Server starting on port " << port_ << "..." << std::endl;
    std::cout.flush();

    running_.store(true);

    crow::SimpleApp app;

    CROW_ROUTE(app, "/v1/devices").methods("GET"_method)([this]() {
        OxDeviceState devices[OX_MAX_DEVICES];
        uint32_t device_count;
        simulator_->GetAllDevices(devices, &device_count);

        crow::json::wvalue response;
        crow::json::wvalue paths_array(crow::json::type::List);
        for (uint32_t i = 0; i < device_count; ++i) {
            paths_array[i] = devices[i].user_path;
        }
        response["paths"] = std::move(paths_array);
        return crow::response(response);
    });

    CROW_ROUTE(app, "/v1/devices/<path>").methods("GET"_method)([this](const std::string& user_path) {
        OxDeviceState devices[OX_MAX_DEVICES];
        uint32_t device_count;
        simulator_->GetAllDevices(devices, &device_count);

        // prepend '/' to user_path since it'll be missing
        std::string full_user_path = "/" + user_path;

        for (uint32_t i = 0; i < device_count; ++i) {
            if (std::strcmp(devices[i].user_path, full_user_path.c_str()) == 0) {
                crow::json::wvalue response;
                response["active"] = devices[i].is_active;
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

    CROW_ROUTE(app, "/v1/devices/<path>")
        .methods("PUT"_method)([this](const crow::request& req, const std::string& user_path) {
            auto json = crow::json::load(req.body);
            if (!json) {
                return crow::response(400, "Invalid JSON");
            }

            if (!json.has("position") || !json.has("orientation") || !json["position"].has("x") ||
                !json["position"].has("y") || !json["position"].has("z") || !json["orientation"].has("x") ||
                !json["orientation"].has("y") || !json["orientation"].has("z") || !json["orientation"].has("w")) {
                return crow::response(400, "Missing required fields: position{x,y,z}, orientation{x,y,z,w}");
            }

            // prepend '/' to user_path since it'll be missing
            std::string full_user_path = "/" + user_path;

            bool is_active = json.has("active") ? json["active"].b() : true;

            OxPose pose;
            pose.position.x = json["position"]["x"].d();
            pose.position.y = json["position"]["y"].d();
            pose.position.z = json["position"]["z"].d();
            pose.orientation.x = json["orientation"]["x"].d();
            pose.orientation.y = json["orientation"]["y"].d();
            pose.orientation.z = json["orientation"]["z"].d();
            pose.orientation.w = json["orientation"]["w"].d();

            simulator_->SetDevicePose(full_user_path.c_str(), pose, is_active);
            return crow::response(200, "OK");
        });

    CROW_ROUTE(app, "/v1/states/<path>").methods("GET"_method)([this](const std::string& binding_path) {
        // prepend '/' to binding_path since it'll be missing
        std::string full_binding_path = "/" + binding_path;

        auto [user_path, component_path] = SplitBindingPath(full_binding_path);
        if (user_path.empty() || component_path.empty()) {
            return crow::response(400, "Invalid binding path");
        }

        OxInputComponentState state;
        OxComponentResult result =
            simulator_->GetInputComponentState(user_path.c_str(), component_path.c_str(), &state);

        if (result != OX_COMPONENT_AVAILABLE) {
            return crow::response(404, "Component not available");
        }

        crow::json::wvalue response;
        response["boolean_value"] = state.boolean_value;
        response["float_value"] = state.float_value;
        response["x"] = state.x;
        response["y"] = state.y;

        return crow::response(response);
    });

    CROW_ROUTE(app, "/v1/states/<path>")
        .methods("PUT"_method)([this](const crow::request& req, const std::string& binding_path) {
            auto json = crow::json::load(req.body);
            if (!json) {
                return crow::response(400, "Invalid JSON");
            }

            if (!json.has("value")) {
                return crow::response(400, "Missing required field: value");
            }

            float value = 0.0f;
            if (json["value"].t() == crow::json::type::Number) {
                value = json["value"].d();
            } else if (json["value"].t() == crow::json::type::True || json["value"].t() == crow::json::type::False) {
                value = json["value"].b() ? 1.0f : 0.0f;
            }

            // prepend '/' to binding_path since it'll be missing
            std::string full_binding_path = "/" + binding_path;

            auto [user_path, component_path] = SplitBindingPath(full_binding_path);
            if (user_path.empty() || component_path.empty()) {
                return crow::response(400, "Invalid binding path");
            }

            simulator_->SetInputComponent(user_path.c_str(), component_path.c_str(), value);
            return crow::response(200, "OK");
        });

    CROW_ROUTE(app, "/")
    ([]() {
        return "ox Simulator API Server\n\nAvailable endpoints:\n"
               "  GET /v1/devices\n"
               "  GET /v1/devices/<user_path>\n"
               "  PUT /v1/devices/<user_path>\n"
               "  GET /v1/states/<binding_path>\n"
               "  PUT /v1/states/<binding_path>\n";
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
