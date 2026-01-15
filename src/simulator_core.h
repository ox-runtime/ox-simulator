#pragma once

#include <ox_driver.h>

#include <atomic>
#include <mutex>
#include <string>

#include "device_profiles.h"

namespace ox_sim {

// Shared device state (written by API/GUI, read by driver)
struct DeviceState {
    // HMD pose
    OxPose hmd_pose;

    // Controller states
    OxControllerState controllers[2];  // Left, Right

    // Input components (simplified - stores common inputs)
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

    InputState left_input;
    InputState right_input;

    // Connection status
    std::atomic<bool> hmd_connected;
    std::atomic<bool> left_controller_connected;
    std::atomic<bool> right_controller_connected;
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
    void GetControllerState(uint32_t controller_index, OxControllerState* out_state);
    OxComponentResult GetInputComponentState(uint32_t controller_index, const char* component_path,
                                             OxInputComponentState* out_state);

    // Update device state (called by API/GUI)
    void SetHMDPose(const OxPose& pose);
    void SetControllerPose(uint32_t controller_index, const OxPose& pose, bool is_active);
    void SetInputComponent(uint32_t controller_index, const char* component_path, float value,
                           bool boolean_value = false);

    // Connection status
    bool IsHMDConnected() const { return state_.hmd_connected.load(); }
    void SetHMDConnected(bool connected) { state_.hmd_connected.store(connected); }

    // Get device state pointer for direct access (for API server)
    DeviceState* GetDeviceState() { return &state_; }
    std::mutex& GetStateMutex() { return state_mutex_; }

   private:
    const DeviceProfile* profile_;
    DeviceState state_;
    mutable std::mutex state_mutex_;

    // Helper to parse component path
    bool ParseComponentPath(const char* component_path, std::string& component_name);
};

}  // namespace ox_sim
