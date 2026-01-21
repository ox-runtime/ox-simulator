#include <ox_driver.h>

#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "crow/json.h"
#include "device_profiles.h"
#include "gui_window.h"
#include "http_server.h"
#include "simulator_core.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

using namespace ox_sim;

// Configuration structure
struct SimulatorConfig {
    std::string device = "oculus_quest_2";
    std::string mode = "api";
    int api_port = 8765;
};

// Global simulator state
static SimulatorCore g_simulator;
static HttpServer g_http_server;
static GuiWindow g_gui_window;
static const DeviceProfile* g_device_profile = nullptr;
static SimulatorConfig g_config;

// Helper function to read config file
static bool LoadConfig(const std::string& config_path) {
    std::ifstream file(config_path);
    if (!file.is_open()) {
        std::cout << "Config file not found at: " << config_path << std::endl;
        std::cout << "Using defaults: device=oculus_quest_2, mode=api, port=8765" << std::endl;
        return false;
    }

    // Read entire file into string
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string config_str = buffer.str();
    file.close();

    // Parse JSON using CROW
    auto json = crow::json::load(config_str);
    if (!json) {
        std::cerr << "ERROR: Invalid JSON in config file: " << config_path << std::endl;
        std::cerr << "Using default configuration" << std::endl;
        return false;
    }

    // Extract configuration with validation
    if (json.has("device") && json["device"].t() == crow::json::type::String) {
        g_config.device = json["device"].s();
    }

    if (json.has("mode") && json["mode"].t() == crow::json::type::String) {
        g_config.mode = json["mode"].s();
        // Validate mode
        if (g_config.mode != "api" && g_config.mode != "gui") {
            std::cerr << "WARNING: Invalid mode '" << g_config.mode << "', defaulting to 'api'" << std::endl;
            g_config.mode = "api";
        }
    }

    if (json.has("api_port") && json["api_port"].t() == crow::json::type::Number) {
        g_config.api_port = static_cast<int>(json["api_port"].d());
        // Validate port range
        if (g_config.api_port < 1024 || g_config.api_port > 65535) {
            std::cerr << "WARNING: Invalid port " << g_config.api_port << ", using default 8765" << std::endl;
            g_config.api_port = 8765;
        }
    }

    // Set device profile
    g_device_profile = GetDeviceProfileByName(g_config.device);
    if (!g_device_profile) {
        std::cout << "Unknown device: " << g_config.device << ", defaulting to Quest 2" << std::endl;
        g_device_profile = &GetDeviceProfile(DeviceType::OCULUS_QUEST_2);
        g_config.device = "oculus_quest_2";
    }

    std::cout << "Loaded config: device=" << g_config.device << ", mode=" << g_config.mode
              << ", port=" << g_config.api_port << std::endl;

    return true;
}

// Helper to get module directory path
static std::filesystem::path get_module_path() {
#ifdef _WIN32
    HMODULE module = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCWSTR>(&get_module_path), &module);

    wchar_t path[MAX_PATH];
    GetModuleFileNameW(module, path, MAX_PATH);
    return std::filesystem::path(path).parent_path();
#else
    Dl_info info{};
    dladdr(reinterpret_cast<void*>(&get_module_path), &info);
    return std::filesystem::path(info.dli_fname).parent_path();
#endif
}

// Get the config file path (same directory as driver)
static std::string GetConfigPath() { return (get_module_path() / "config.json").string(); }

// ===== Driver Callbacks =====

static int simulator_initialize(void) {
    std::cout << "=== ox Simulator Driver ===" << std::endl;

    // Load configuration
    std::string config_path = GetConfigPath();
    LoadConfig(config_path);

    if (!g_device_profile) {
        g_device_profile = &GetDeviceProfile(DeviceType::OCULUS_QUEST_2);
    }

    std::cout << "Simulating device: " << g_device_profile->name << std::endl;

    // Initialize simulator core
    if (!g_simulator.Initialize(g_device_profile)) {
        std::cerr << "Failed to initialize simulator core" << std::endl;
        return 0;
    }

    // Start interface based on mode
    if (g_config.mode == "api") {
        std::cout << "Starting HTTP API server on port " << g_config.api_port << "..." << std::endl;
        if (!g_http_server.Start(&g_simulator, &g_device_profile, g_config.api_port)) {
            std::cerr << "Failed to start HTTP server" << std::endl;
            return 0;
        }
        std::cout << "HTTP API server started successfully" << std::endl;
        std::cout << "Use API endpoints to control the simulator:" << std::endl;
        std::cout << "  GET/PUT  http://localhost:" << g_config.api_port << "/v1/profile" << std::endl;
        std::cout << "  GET/PUT  http://localhost:" << g_config.api_port << "/v1/devices" << std::endl;
        std::cout << "  GET/PUT  http://localhost:" << g_config.api_port << "/v1/devices/user/head" << std::endl;
        std::cout << "  GET/PUT  http://localhost:" << g_config.api_port << "/v1/devices/user/hand/right" << std::endl;
        std::cout << "  GET/PUT  http://localhost:" << g_config.api_port
                  << "/v1/inputs/user/hand/right/input/trigger/value" << std::endl;
    } else if (g_config.mode == "gui") {
        std::cout << "Starting GUI interface..." << std::endl;
        if (!g_gui_window.Start(&g_simulator)) {
            std::cerr << "Failed to start GUI window" << std::endl;
            return 0;
        }
    } else {
        std::cerr << "Unknown mode: " << g_config.mode << std::endl;
        return 0;
    }

    std::cout << "Simulator driver initialized successfully" << std::endl;
    return 1;
}

static void simulator_shutdown(void) {
    std::cout << "Shutting down simulator driver..." << std::endl;

    g_http_server.Stop();
    g_gui_window.Stop();
    g_simulator.Shutdown();

    std::cout << "Simulator driver shut down" << std::endl;
}

static int simulator_is_device_connected(void) {
    // Simulator is always "connected"
    return 1;
}

static void simulator_get_device_info(OxDeviceInfo* info) {
    if (!g_device_profile) {
        return;
    }

    std::strncpy(info->name, g_device_profile->name, sizeof(info->name) - 1);
    info->name[sizeof(info->name) - 1] = '\0';

    std::strncpy(info->manufacturer, g_device_profile->manufacturer, sizeof(info->manufacturer) - 1);
    info->manufacturer[sizeof(info->manufacturer) - 1] = '\0';

    // Generate unique serial number
    std::string serial = std::string(g_device_profile->serial_prefix) + "-12345";
    std::strncpy(info->serial, serial.c_str(), sizeof(info->serial) - 1);
    info->serial[sizeof(info->serial) - 1] = '\0';

    info->vendor_id = g_device_profile->vendor_id;
    info->product_id = g_device_profile->product_id;
}

static void simulator_get_display_properties(OxDisplayProperties* props) {
    if (!g_device_profile) {
        return;
    }

    props->display_width = g_device_profile->display_width;
    props->display_height = g_device_profile->display_height;
    props->recommended_width = g_device_profile->recommended_width;
    props->recommended_height = g_device_profile->recommended_height;
    props->refresh_rate = g_device_profile->refresh_rate;

    props->fov.angle_left = g_device_profile->fov_left;
    props->fov.angle_right = g_device_profile->fov_right;
    props->fov.angle_up = g_device_profile->fov_up;
    props->fov.angle_down = g_device_profile->fov_down;
}

static void simulator_get_tracking_capabilities(OxTrackingCapabilities* caps) {
    if (!g_device_profile) {
        return;
    }

    caps->has_position_tracking = g_device_profile->has_position_tracking ? 1 : 0;
    caps->has_orientation_tracking = g_device_profile->has_orientation_tracking ? 1 : 0;
}

static void simulator_update_view_pose(int64_t predicted_time, uint32_t eye_index, OxPose* out_pose) {
    // Get HMD pose from device list (HMD is at /user/head)
    OxDeviceState devices[OX_MAX_DEVICES];
    uint32_t device_count;
    g_simulator.GetAllDevices(devices, &device_count);

    // Find HMD device
    OxPose hmd_pose = {{0.0f, 1.6f, 0.0f}, {0.0f, 0.0f, 0.0f, 1.0f}};  // Default origin at eye level
    for (uint32_t i = 0; i < device_count; i++) {
        if (std::strcmp(devices[i].user_path, "/user/head") == 0) {
            hmd_pose = devices[i].pose;
            break;
        }
    }
    // If no HMD found (e.g., vive tracker profile), just use origin
    // Most tracker applications don't render, so view pose doesn't matter

    // Apply IPD offset (typical IPD is ~63mm = 0.063m)
    float ipd = 0.063f;
    float eye_offset = (eye_index == 0) ? -ipd / 2.0f : ipd / 2.0f;

    *out_pose = hmd_pose;
    out_pose->position.x += eye_offset;
}

static void simulator_update_devices(int64_t predicted_time, OxDeviceState* out_states, uint32_t* out_count) {
    if (!g_device_profile) {
        *out_count = 0;
        return;
    }

    g_simulator.GetAllDevices(out_states, out_count);
}

static OxComponentResult simulator_get_input_component_state(int64_t predicted_time, const char* user_path,
                                                             const char* component_path,
                                                             OxInputComponentState* out_state) {
    if (!g_device_profile) {
        return OX_COMPONENT_UNAVAILABLE;
    }

    return g_simulator.GetInputComponentState(user_path, component_path, out_state);
}

static uint32_t simulator_get_interaction_profiles(const char** out_profiles, uint32_t max_count) {
    if (!g_device_profile || max_count == 0) {
        return 0;
    }

    out_profiles[0] = g_device_profile->interaction_profile;
    return 1;
}

// ===== Driver Registration =====

extern "C" OX_DRIVER_EXPORT int ox_driver_register(OxDriverCallbacks* callbacks) {
    if (!callbacks) {
        return 0;
    }

    callbacks->initialize = simulator_initialize;
    callbacks->shutdown = simulator_shutdown;
    callbacks->is_device_connected = simulator_is_device_connected;
    callbacks->get_device_info = simulator_get_device_info;
    callbacks->get_display_properties = simulator_get_display_properties;
    callbacks->get_tracking_capabilities = simulator_get_tracking_capabilities;
    callbacks->update_view_pose = simulator_update_view_pose;
    callbacks->update_devices = simulator_update_devices;
    callbacks->get_input_component_state = simulator_get_input_component_state;
    callbacks->get_interaction_profiles = simulator_get_interaction_profiles;

    return 1;
}
