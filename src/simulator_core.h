#pragma once

#include <ox_driver.h>

#include <atomic>
#include <map>
#include <mutex>
#include <string>

#include "device_profiles.h"

namespace ox_sim {

// 2D vector for input components (since OxDriver uses separate x/y fields)
struct OxVector2f {
    float x;
    float y;
};

// Input state for a single device (dynamically sized based on components)
struct DeviceInputState {
    std::map<std::string, float> float_values;      // component_path -> value
    std::map<std::string, bool> boolean_values;     // component_path -> value
    std::map<std::string, OxVector2f> vec2_values;  // component_path -> {x, y}
};

// Shared device state (written by API/GUI, read by driver)
struct DeviceState {
    // Tracked devices: device[0] = HMD (/user/head), device[1+] = controllers, trackers, etc.
    OxDeviceState devices[OX_MAX_DEVICES];
    uint32_t device_count;

    // Input state per device (indexed same as devices array)
    // Using dynamic map instead of fixed struct
    DeviceInputState device_inputs[OX_MAX_DEVICES];
};

class SimulatorCore {
   public:
    SimulatorCore();
    ~SimulatorCore();

    // Initialize with device profile
    bool Initialize(const DeviceProfile* profile);
    void Shutdown();

    // Get current device profile
    const DeviceProfile* GetProfile() const { return profile_; }

    // Switch to a different device profile (reinitializes devices)
    bool SwitchDevice(const DeviceProfile* profile);

    // Device state access (thread-safe)
    void GetAllDevices(OxDeviceState* out_states, uint32_t* out_count);
    OxComponentResult GetInputComponentState(const char* user_path, const char* component_path,
                                             OxInputComponentState* out_state);

    // Update device state (called by API/GUI)
    void SetDevicePose(const char* user_path, const OxPose& pose, bool is_active);
    void SetInputComponent(const char* user_path, const char* component_path, float value);

    // Get device state pointer for direct access (for API server)
    DeviceState* GetDeviceState() { return &state_; }
    std::mutex& GetStateMutex() { return state_mutex_; }

   private:
    const DeviceProfile* profile_;
    DeviceState state_;
    mutable std::mutex state_mutex_;
};

}  // namespace ox_sim
