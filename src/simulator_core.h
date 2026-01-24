#pragma once

#include <ox_driver.h>

#include <atomic>
#include <map>
#include <mutex>
#include <string>
#include <utility>
#include <variant>

#include "device_profiles.h"

namespace ox_sim {

// Variant type to hold any input component value
using InputValue = std::variant<bool, float, OxVector2f>;

// Input state for a single device (dynamically sized based on components)
struct DeviceInputState {
    std::vector<InputValue> values;  // Indexed by component index
};

// Shared device state (written by API/GUI, read by driver)
struct DeviceState {
    // Tracked devices: device[0] = HMD (/user/head), device[1+] = controllers, trackers, etc.
    OxDeviceState devices[OX_MAX_DEVICES];
    uint32_t device_count;

    // Input state per device (indexed same as devices array)
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

    // Device state access
    void UpdateAllDevices(OxDeviceState* out_states, uint32_t* out_count);
    bool GetDevicePose(const char* user_path, OxPose* out_pose, bool* out_is_active);

    // Input state access
    OxComponentResult GetInputStateBoolean(const char* user_path, const char* component_path, bool* out_value);
    OxComponentResult GetInputStateFloat(const char* user_path, const char* component_path, float* out_value);
    OxComponentResult GetInputStateVec2(const char* user_path, const char* component_path, OxVector2f* out_value);

    // Update device state
    void SetDevicePose(const char* user_path, const OxPose& pose, bool is_active);

    // Update input state
    void SetInputStateBoolean(const char* user_path, const char* component_path, bool value);
    void SetInputStateFloat(const char* user_path, const char* component_path, float value);
    void SetInputStateVec2(const char* user_path, const char* component_path, const OxVector2f& value);

    // Helper functions
    const DeviceDef* FindDeviceDefByUserPath(const char* user_path) const;
    std::pair<int32_t, ComponentType> FindComponentInfo(const DeviceDef* device_def, const char* component_path) const;

   private:
    // Template implementations for input state access
    template <ComponentType CT, typename T>
    OxComponentResult GetInputState(const char* user_path, const char* component_path, T* out_value);

    template <ComponentType CT, typename T>
    void SetInputState(const char* user_path, const char* component_path, const T& value);

    // Helper functions
    int FindDeviceIndexByUserPath(const char* user_path) const;
    std::tuple<DeviceInputState*, int32_t, ComponentType> ValidateDeviceAndComponent(const char* user_path,
                                                                                     const char* component_path);

    // Member variables
    const DeviceProfile* profile_;
    DeviceState state_;
    mutable std::mutex state_mutex_;
};

}  // namespace ox_sim
