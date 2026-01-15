#pragma once

#include <ox_driver.h>

#include <string>

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

    // Controller support
    bool has_controllers;
    const char* interaction_profile;  // e.g., "/interaction_profiles/oculus/touch_controller"
};

// Get device profile by type
const DeviceProfile& GetDeviceProfile(DeviceType type);

// Get device profile by name (for config file)
const DeviceProfile* GetDeviceProfileByName(const std::string& name);

}  // namespace ox_sim
