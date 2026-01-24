#include "simulator_core.h"

#include <cmath>
#include <cstring>
#include <unordered_map>

namespace ox_sim {

SimulatorCore::SimulatorCore() : profile_(nullptr), state_{} { state_.device_count = 0; }

SimulatorCore::~SimulatorCore() { Shutdown(); }

// Helper to find device index in state by user path
int SimulatorCore::FindDeviceIndexByUserPath(const char* user_path) const {
    for (uint32_t i = 0; i < state_.device_count && i < OX_MAX_DEVICES; i++) {
        if (std::strcmp(state_.devices[i].user_path, user_path) == 0) {
            return i;
        }
    }
    return -1;
}

// Helper to find device definition in profile by user path
const DeviceDef* SimulatorCore::FindDeviceDefByUserPath(const char* user_path) const {
    if (!profile_) {
        return nullptr;
    }

    for (const auto& dev : profile_->devices) {
        if (std::strcmp(dev.user_path, user_path) == 0) {
            return &dev;
        }
    }
    return nullptr;
}

// Helper to find component information in device definition
std::pair<int32_t, ComponentType> SimulatorCore::FindComponentInfo(const DeviceDef* device_def,
                                                                   const char* component_path) const {
    if (!device_def) {
        return {-1, ComponentType::FLOAT};
    }

    int32_t index = 0;
    for (const auto& comp : device_def->components) {
        if (std::strcmp(comp.path, component_path) == 0) {
            return {index, comp.type};
        }
        index++;
    }
    return {-1, ComponentType::FLOAT};
}

// Helper to validate device and component, returning input state pointer and component info
std::tuple<DeviceInputState*, int32_t, ComponentType> SimulatorCore::ValidateDeviceAndComponent(
    const char* user_path, const char* component_path) {
    int device_index = FindDeviceIndexByUserPath(user_path);
    const DeviceDef* device_def = FindDeviceDefByUserPath(user_path);
    if (device_index < 0 || !device_def) {
        return {nullptr, -1, ComponentType::FLOAT};
    }

    auto [comp_index, comp_type] = FindComponentInfo(device_def, component_path);
    if (comp_index == -1) {
        return {nullptr, -1, ComponentType::FLOAT};
    }

    return {&state_.device_inputs[device_index], comp_index, comp_type};
}

bool SimulatorCore::Initialize(const DeviceProfile* profile) {
    if (!profile) {
        return false;
    }

    std::lock_guard<std::mutex> lock(state_mutex_);
    profile_ = profile;

    // Initialize devices from profile
    state_.device_count = std::min(static_cast<size_t>(OX_MAX_DEVICES), profile->devices.size());

    for (uint32_t i = 0; i < state_.device_count; i++) {
        const DeviceDef& dev_def = profile->devices[i];

        // Set user path
        std::strncpy(state_.devices[i].user_path, dev_def.user_path, sizeof(state_.devices[i].user_path) - 1);
        state_.devices[i].user_path[sizeof(state_.devices[i].user_path) - 1] = '\0';

        // Set active state
        state_.devices[i].is_active = dev_def.always_active ? 1 : 0;

        // Set default pose from profile
        state_.devices[i].pose = dev_def.default_pose;

        // Initialize input state (all components to zero/false)
        size_t max_component_index = dev_def.components.size();
        state_.device_inputs[i].values.resize(max_component_index);

        // Pre-populate all components for this device
        for (size_t comp_index = 0; comp_index < dev_def.components.size(); ++comp_index) {
            const auto& component = dev_def.components[comp_index];
            switch (component.type) {
                case ComponentType::FLOAT:
                    state_.device_inputs[i].values[comp_index] = 0.0f;
                    break;
                case ComponentType::BOOLEAN:
                    state_.device_inputs[i].values[comp_index] = false;
                    break;
                case ComponentType::VEC2:
                    state_.device_inputs[i].values[comp_index] = OxVector2f{0.0f, 0.0f};
                    break;
            }
        }
    }

    return true;
}

bool SimulatorCore::SwitchDevice(const DeviceProfile* profile) {
    Shutdown();
    return Initialize(profile);
}

void SimulatorCore::Shutdown() {
    std::lock_guard<std::mutex> lock(state_mutex_);
    profile_ = nullptr;
    state_.device_count = 0;
}

void SimulatorCore::UpdateAllDevices(OxDeviceState* out_states, uint32_t* out_count) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    *out_count = state_.device_count;
    for (uint32_t i = 0; i < state_.device_count && i < OX_MAX_DEVICES; i++) {
        out_states[i] = state_.devices[i];
    }
}

bool SimulatorCore::GetDevicePose(const char* user_path, OxPose* out_pose, bool* out_is_active) {
    std::lock_guard<std::mutex> lock(state_mutex_);

    int device_index = FindDeviceIndexByUserPath(user_path);
    if (device_index < 0) {
        return false;
    }

    *out_pose = state_.devices[device_index].pose;
    *out_is_active = state_.devices[device_index].is_active != 0;
    return true;
}

void SimulatorCore::SetDevicePose(const char* user_path, const OxPose& pose, bool is_active) {
    std::lock_guard<std::mutex> lock(state_mutex_);

    // Find the device by user path
    int device_index = FindDeviceIndexByUserPath(user_path);
    const DeviceDef* device_def = FindDeviceDefByUserPath(user_path);
    if (device_index < 0) {
        return;
    }

    state_.devices[device_index].pose = pose;
    bool device_always_active = device_def ? device_def->always_active : false;
    state_.devices[device_index].is_active = device_always_active ? 1 : (is_active ? 1 : 0);
}

template <ComponentType CT, typename T>
OxComponentResult SimulatorCore::GetInputState(const char* user_path, const char* component_path, T* out_value) {
    std::lock_guard<std::mutex> lock(state_mutex_);

    auto [input, comp_index, comp_type] = ValidateDeviceAndComponent(user_path, component_path);
    if (!input || comp_index == -1 || comp_type != CT) {
        return OX_COMPONENT_UNAVAILABLE;
    }

    *out_value = std::get<T>(input->values[comp_index]);
    return OX_COMPONENT_AVAILABLE;
}

template <ComponentType CT, typename T>
void SimulatorCore::SetInputState(const char* user_path, const char* component_path, const T& value) {
    std::lock_guard<std::mutex> lock(state_mutex_);

    auto [input, comp_index, comp_type] = ValidateDeviceAndComponent(user_path, component_path);
    if (!input || comp_index == -1 || comp_type != CT) {
        return;
    }

    input->values[comp_index] = value;
}

OxComponentResult SimulatorCore::GetInputStateBoolean(const char* user_path, const char* component_path,
                                                      bool* out_value) {
    return GetInputState<ComponentType::BOOLEAN, bool>(user_path, component_path, out_value);
}

OxComponentResult SimulatorCore::GetInputStateFloat(const char* user_path, const char* component_path,
                                                    float* out_value) {
    return GetInputState<ComponentType::FLOAT, float>(user_path, component_path, out_value);
}

OxComponentResult SimulatorCore::GetInputStateVec2(const char* user_path, const char* component_path,
                                                   OxVector2f* out_value) {
    return GetInputState<ComponentType::VEC2, OxVector2f>(user_path, component_path, out_value);
}

void SimulatorCore::SetInputStateBoolean(const char* user_path, const char* component_path, bool value) {
    SetInputState<ComponentType::BOOLEAN, bool>(user_path, component_path, value);
}

void SimulatorCore::SetInputStateFloat(const char* user_path, const char* component_path, float value) {
    SetInputState<ComponentType::FLOAT, float>(user_path, component_path, value);
}

void SimulatorCore::SetInputStateVec2(const char* user_path, const char* component_path, const OxVector2f& value) {
    SetInputState<ComponentType::VEC2, OxVector2f>(user_path, component_path, value);
}

}  // namespace ox_sim
