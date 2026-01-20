#pragma once

#include <ox_driver.h>

#include <atomic>
#include <mutex>
#include <string>

#include "device_profiles.h"

namespace ox_sim {

// Shared device state (written by API/GUI, read by driver)
struct DeviceState {
    // Tracked devices: device[0] = HMD (/user/head), device[1+] = controllers, trackers, etc.
    OxDeviceState devices[OX_MAX_DEVICES];
    uint32_t device_count;

    // Input state per device (indexed same as devices array)
    struct InputState {
        // Trigger
        float trigger_value;
        bool trigger_click;
        bool trigger_touch;

        // Grip
        float grip_value;
        bool grip_click;

        // Thumbstick
        float thumbstick_x;
        float thumbstick_y;
        bool thumbstick_click;
        bool thumbstick_touch;

        // Buttons (A/B or X/Y depending on controller)
        bool button_a_click;
        bool button_a_touch;
        bool button_b_click;
        bool button_b_touch;

        // Menu/System button
        bool menu_click;
    };

    InputState device_inputs[OX_MAX_DEVICES];
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

    // Device state access (thread-safe)
    void GetHMDPose(OxPose* out_pose);
    void GetAllDevices(OxDeviceState* out_states, uint32_t* out_count);
    OxComponentResult GetInputComponentState(const char* user_path, const char* component_path,
                                             OxInputComponentState* out_state);

    // Update device state (called by API/GUI)
    void SetHMDPose(const OxPose& pose);
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
