#include <ox_driver.h>

#include <cstring>
#include <iostream>
#include <string>

#include "config.hpp"
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

// Global simulator state
static SimulatorCore g_simulator;
static HttpServer g_http_server;
static GuiWindow g_gui_window;
SimulatorConfig g_config;
const ox_sim::DeviceProfile* g_device_profile = nullptr;

// ===== Driver Callbacks =====

static int simulator_initialize(void) {
    std::cout << "=== ox Simulator Driver ===" << std::endl;

    // Load configuration
    std::string config_path = GetConfigPath();
    LoadConfig(config_path);

    // Set device profile based on config
    g_device_profile = GetDeviceProfileByName(g_config.device);
    if (!g_device_profile) {
        std::cout << "Unknown device: " << g_config.device << ", defaulting to Quest 2" << std::endl;
        g_device_profile = &GetDeviceProfile(DeviceType::OCULUS_QUEST_2);
        g_config.device = "oculus_quest_2";
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
