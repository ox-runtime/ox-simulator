#include "http_server.h"

#include <iostream>
#include <sstream>
#include <string>

#include "crow/app.h"
#include "crow/json.h"
#include "device_profiles.h"

namespace ox_sim {

HttpServer::HttpServer()
    : simulator_(nullptr), device_profile_ptr_(nullptr), port_(8765), running_(false), should_stop_(false) {}

HttpServer::~HttpServer() { Stop(); }

bool HttpServer::Start(SimulatorCore* simulator, const DeviceProfile** device_profile_ptr, int port) {
    if (!simulator || !device_profile_ptr) {
        return false;
    }

    if (running_.load()) {
        return false;  // Already running
    }

    std::cout << "Starting HTTP API server on port " << port << "..." << std::endl;

    simulator_ = simulator;
    device_profile_ptr_ = device_profile_ptr;
    port_ = port;
    should_stop_.store(false);

    server_thread_ = std::thread(&HttpServer::ServerThread, this);

    // Detach thread so it runs independently
    server_thread_.detach();

    // Give server time to start
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    if (running_.load()) {
        std::cout << "HTTP API server started successfully" << std::endl;
        std::cout << "Use API endpoints to control the simulator:" << std::endl;
        std::cout << "  GET/PUT  http://localhost:" << port << "/v1/profile" << std::endl;
        std::cout << "  GET/PUT  http://localhost:" << port << "/v1/devices/user/head" << std::endl;
        std::cout << "  GET/PUT  http://localhost:" << port << "/v1/devices/user/hand/right" << std::endl;
        std::cout << "  GET/PUT  http://localhost:" << port << "/v1/inputs/user/hand/right/input/trigger/value"
                  << std::endl;
    }

    return running_.load();
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

    CROW_ROUTE(app, "/v1/devices/<path>").methods("GET"_method)([this](const std::string& user_path) {
        // prepend '/' to user_path since it'll be missing
        std::string full_user_path = "/" + user_path;

        OxPose pose;
        bool is_active;
        if (!simulator_->GetDevicePose(full_user_path.c_str(), &pose, &is_active)) {
            return crow::response(404, "Device not found");
        }

        crow::json::wvalue response;
        response["active"] = is_active;
        response["position"]["x"] = pose.position.x;
        response["position"]["y"] = pose.position.y;
        response["position"]["z"] = pose.position.z;
        response["orientation"]["x"] = pose.orientation.x;
        response["orientation"]["y"] = pose.orientation.y;
        response["orientation"]["z"] = pose.orientation.z;
        response["orientation"]["w"] = pose.orientation.w;
        return crow::response(response);
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

    CROW_ROUTE(app, "/v1/inputs/<path>").methods("GET"_method)([this](const std::string& binding_path) {
        // prepend '/' to binding_path since it'll be missing
        std::string full_binding_path = "/" + binding_path;

        auto [user_path, component_path] = SplitBindingPath(full_binding_path);
        if (user_path.empty() || component_path.empty()) {
            return crow::response(400, "Invalid binding path");
        }

        // Determine component type from device profile
        const DeviceDef* device_def = simulator_->FindDeviceDefByUserPath(user_path.c_str());
        if (!device_def) {
            return crow::response(404, "Device not found");
        }
        auto [comp_index, comp_type] = simulator_->FindComponentInfo(device_def, component_path.c_str());
        if (comp_index == -1) {
            return crow::response(404, "Component not found in device profile");
        }

        crow::json::wvalue response;
        OxComponentResult result;

        // Call the appropriate type-specific function
        if (comp_type == ComponentType::BOOLEAN) {
            bool value = false;
            result = simulator_->GetInputStateBoolean(user_path.c_str(), component_path.c_str(), &value);
            if (result != OX_COMPONENT_AVAILABLE) {
                return crow::response(404, "Component not available");
            }
            response["type"] = "boolean";
            response["value"] = value;
        } else if (comp_type == ComponentType::FLOAT) {
            float value = 0.0f;
            result = simulator_->GetInputStateFloat(user_path.c_str(), component_path.c_str(), &value);
            if (result != OX_COMPONENT_AVAILABLE) {
                return crow::response(404, "Component not available");
            }
            response["type"] = "float";
            response["value"] = value;
        } else {  // VEC2
            OxVector2f vec;
            result = simulator_->GetInputStateVec2(user_path.c_str(), component_path.c_str(), &vec);
            if (result != OX_COMPONENT_AVAILABLE) {
                return crow::response(404, "Component not available");
            }
            response["type"] = "vec2";
            response["x"] = vec.x;
            response["y"] = vec.y;
        }

        return crow::response(response);
    });

    CROW_ROUTE(app, "/v1/inputs/<path>")
        .methods("PUT"_method)([this](const crow::request& req, const std::string& binding_path) {
            auto json = crow::json::load(req.body);
            if (!json) {
                return crow::response(400, "Invalid JSON");
            }

            // prepend '/' to binding_path since it'll be missing
            std::string full_binding_path = "/" + binding_path;

            auto [user_path, component_path] = SplitBindingPath(full_binding_path);
            if (user_path.empty() || component_path.empty()) {
                return crow::response(400, "Invalid binding path");
            }

            // Determine component type from device profile
            const DeviceDef* device_def = simulator_->FindDeviceDefByUserPath(user_path.c_str());
            if (!device_def) {
                return crow::response(404, "Device not found");
            }
            auto [comp_index, comp_type] = simulator_->FindComponentInfo(device_def, component_path.c_str());
            if (comp_index == -1) {
                return crow::response(404, "Component not found in device profile");
            }

            // Handle different component types
            switch (comp_type) {
                case ComponentType::BOOLEAN: {
                    if (!json.has("value")) {
                        return crow::response(400, "Missing required field: value");
                    }
                    bool bool_value = false;
                    if (json["value"].t() == crow::json::type::True || json["value"].t() == crow::json::type::False) {
                        bool_value = json["value"].b();
                    } else if (json["value"].t() == crow::json::type::Number) {
                        bool_value = json["value"].d() >= 0.5;
                    } else {
                        return crow::response(400, "Invalid value for boolean component");
                    }
                    simulator_->SetInputStateBoolean(user_path.c_str(), component_path.c_str(), bool_value);
                    break;
                }
                case ComponentType::FLOAT: {
                    if (!json.has("value")) {
                        return crow::response(400, "Missing required field: value");
                    }
                    float float_value = 0.0f;
                    if (json["value"].t() == crow::json::type::Number) {
                        float_value = json["value"].d();
                    } else if (json["value"].t() == crow::json::type::True ||
                               json["value"].t() == crow::json::type::False) {
                        float_value = json["value"].b() ? 1.0f : 0.0f;
                    } else {
                        return crow::response(400, "Invalid value for float component");
                    }
                    simulator_->SetInputStateFloat(user_path.c_str(), component_path.c_str(), float_value);
                    break;
                }
                case ComponentType::VEC2: {
                    OxVector2f vec = {0.0f, 0.0f};

                    // Check if it's an object with x,y fields
                    if (json.has("x") && json.has("y")) {
                        if (json["x"].t() == crow::json::type::Number && json["y"].t() == crow::json::type::Number) {
                            vec.x = json["x"].d();
                            vec.y = json["y"].d();
                        } else {
                            return crow::response(400, "Invalid x,y values for vec2 component");
                        }
                    } else {
                        return crow::response(400, "Missing required fields: x,y for vec2 component");
                    }

                    simulator_->SetInputStateVec2(user_path.c_str(), component_path.c_str(), vec);
                    break;
                }
            }

            return crow::response(200, "OK");
        });

    // Get current device profile
    CROW_ROUTE(app, "/v1/profile").methods("GET"_method)([this]() {
        const DeviceProfile* profile = *device_profile_ptr_;
        if (!profile) {
            return crow::response(500, "No device profile loaded");
        }

        crow::json::wvalue response;
        response["type"] = profile->name;
        response["manufacturer"] = profile->manufacturer;
        response["interaction_profile"] = profile->interaction_profile;

        // List devices
        crow::json::wvalue devices_array(crow::json::type::List);
        for (size_t i = 0; i < profile->devices.size(); ++i) {
            const auto& dev = profile->devices[i];
            crow::json::wvalue dev_obj;
            dev_obj["user_path"] = dev.user_path;
            dev_obj["role"] = dev.role;
            dev_obj["always_active"] = dev.always_active;

            // List components
            crow::json::wvalue components_array(crow::json::type::List);
            for (size_t j = 0; j < dev.components.size(); ++j) {
                const auto& comp = dev.components[j];
                crow::json::wvalue comp_obj;
                comp_obj["path"] = comp.path;
                comp_obj["type"] = (comp.type == ComponentType::FLOAT     ? "float"
                                    : comp.type == ComponentType::BOOLEAN ? "boolean"
                                                                          : "vec2");
                comp_obj["description"] = comp.description;
                components_array[j] = std::move(comp_obj);
            }
            dev_obj["components"] = std::move(components_array);

            devices_array[i] = std::move(dev_obj);
        }
        response["devices"] = std::move(devices_array);

        return crow::response(response);
    });

    // Switch device profile
    CROW_ROUTE(app, "/v1/profile").methods("PUT"_method)([this](const crow::request& req) {
        auto json = crow::json::load(req.body);
        if (!json) {
            return crow::response(400, "Invalid JSON");
        }

        if (!json.has("device") || json["device"].t() != crow::json::type::String) {
            return crow::response(400, "Missing required field: device (string)");
        }

        std::string device_name = json["device"].s();
        const DeviceProfile* new_profile = GetDeviceProfileByName(device_name);
        if (!new_profile) {
            return crow::response(404, "Unknown device: " + device_name);
        }

        // Switch the device
        if (!simulator_->SwitchDevice(new_profile)) {
            return crow::response(500, "Failed to switch device");
        }

        // Update the global pointer
        *device_profile_ptr_ = new_profile;

        crow::json::wvalue response;
        response["status"] = "ok";
        response["device"] = new_profile->name;
        response["interaction_profile"] = new_profile->interaction_profile;
        return crow::response(response);
    });

    CROW_ROUTE(app, "/")
    ([]() {
        return "ox Simulator API Server\n\nAvailable endpoints:\n"
               "  GET  /v1/profile              - Get current device profile (devices and inputs)\n"
               "  PUT  /v1/profile              - Switch device profile\n"
               "  GET  /v1/devices/<user_path>  - Get device pose\n"
               "  PUT  /v1/devices/<user_path>  - Set device pose\n"
               "  GET  /v1/inputs/<binding_path>  - Get input component state\n"
               "  PUT  /v1/inputs/<binding_path>  - Set input component state\n";
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
