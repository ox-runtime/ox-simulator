#pragma once

#include <ox_driver.h>

#include <string>
#include <vector>

namespace ox_sim {

// Device type enumeration - extensible for future devices
enum class DeviceType {
    OCULUS_QUEST_2,
    OCULUS_QUEST_3,
    HTC_VIVE,
    VALVE_INDEX,
    VIVE_TRACKER,
    // Add more device types here in the future
};

// Input component type
enum class ComponentType {
    FLOAT,    // Analog values: triggers, grips (0.0 to 1.0)
    BOOLEAN,  // Digital values: clicks, touches (0 or 1)
    VEC2      // 2D vectors: thumbsticks, trackpads (-1.0 to 1.0)
};

// Component definition for a device
struct ComponentDef {
    const char* path;  // e.g., "/input/trigger/value"
    ComponentType type;
    const char* description;  // Human-readable description
};

// Device definition (HMD, controller, tracker, etc.)
struct DeviceDef {
    const char* user_path;                 // e.g., "/user/head", "/user/hand/left"
    const char* role;                      // e.g., "hmd", "left_controller", "right_controller"
    bool is_tracked;                       // Whether this device has pose tracking
    bool always_active;                    // Whether device is always active (e.g., HMD)
    std::vector<ComponentDef> components;  // Input components for this device
};

// Device profile containing all static properties
struct DeviceProfile {
    DeviceType type;

    // Device info
    const char* name;
    const char* manufacturer;
    const char* serial_prefix;
    uint32_t vendor_id;
    uint32_t product_id;

    // Display properties
    uint32_t display_width;
    uint32_t display_height;
    uint32_t recommended_width;
    uint32_t recommended_height;
    float refresh_rate;

    // Field of view (radians)
    float fov_left;
    float fov_right;
    float fov_up;
    float fov_down;

    // Tracking capabilities
    bool has_position_tracking;
    bool has_orientation_tracking;

    // Interaction profile
    const char* interaction_profile;  // e.g., "/interaction_profiles/oculus/touch_controller"

    // Devices that make up this system (HMD, controllers, trackers, etc.)
    std::vector<DeviceDef> devices;
};

// Get device profile by type
const DeviceProfile& GetDeviceProfile(DeviceType type);

// Get device profile by name (for config file)
const DeviceProfile* GetDeviceProfileByName(const std::string& name);

// Get device type by name
DeviceType GetDeviceTypeByName(const std::string& name);

}  // namespace ox_sim
