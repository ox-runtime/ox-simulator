#include "simulator_core.h"

#include <cmath>
#include <cstring>
#include <unordered_map>

namespace ox_sim {

SimulatorCore::SimulatorCore() : profile_(nullptr), state_{} { state_.device_count = 0; }

SimulatorCore::~SimulatorCore() { Shutdown(); }

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
        state_.devices[i].user_path[sizeof(state_.devices[i].user_path) - 1] = '\\0';

        // Set active state
        state_.devices[i].is_active = dev_def.always_active ? 1 : 0;

        // Set default pose from profile
        state_.devices[i].pose = dev_def.default_pose;

        // Initialize input state (all components to zero/false)
        state_.device_inputs[i].float_values.clear();
        state_.device_inputs[i].boolean_values.clear();
        state_.device_inputs[i].vec2_values.clear();

        // Pre-populate all components for this device
        for (const auto& component : dev_def.components) {
            switch (component.type) {
                case ComponentType::FLOAT:
                    state_.device_inputs[i].float_values[component.path] = 0.0f;
                    break;
                case ComponentType::BOOLEAN:
                    state_.device_inputs[i].boolean_values[component.path] = false;
                    break;
                case ComponentType::VEC2:
                    state_.device_inputs[i].vec2_values[component.path] = {0.0f, 0.0f};
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

void SimulatorCore::GetAllDevices(OxDeviceState* out_states, uint32_t* out_count) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    *out_count = state_.device_count;
    for (uint32_t i = 0; i < state_.device_count && i < OX_MAX_DEVICES; i++) {
        out_states[i] = state_.devices[i];
    }
}

OxComponentResult SimulatorCore::GetInputComponentState(const char* user_path, const char* component_path,
                                                        OxInputComponentState* out_state) {
    std::lock_guard<std::mutex> lock(state_mutex_);

    // Find the device by user path
    int device_index = -1;
    for (uint32_t i = 0; i < state_.device_count && i < OX_MAX_DEVICES; i++) {
        if (std::strcmp(state_.devices[i].user_path, user_path) == 0) {
            device_index = i;
            break;
        }
    }

    if (device_index < 0) {
        return OX_COMPONENT_UNAVAILABLE;
    }

    // Validate that this component exists for this device in the profile
    if (!profile_) {
        return OX_COMPONENT_UNAVAILABLE;
    }

    const DeviceDef* device_def = nullptr;
    for (const auto& dev : profile_->devices) {
        if (std::strcmp(dev.user_path, user_path) == 0) {
            device_def = &dev;
            break;
        }
    }

    if (!device_def) {
        return OX_COMPONENT_UNAVAILABLE;
    }

    // Check if component exists in device definition
    bool component_exists = false;
    ComponentType comp_type = ComponentType::FLOAT;
    for (const auto& comp : device_def->components) {
        if (std::strcmp(comp.path, component_path) == 0) {
            component_exists = true;
            comp_type = comp.type;
            break;
        }
    }

    if (!component_exists) {
        return OX_COMPONENT_UNAVAILABLE;
    }

    const DeviceInputState& input = state_.device_inputs[device_index];
    memset(out_state, 0, sizeof(OxInputComponentState));

    // Retrieve value from dynamic storage
    switch (comp_type) {
        case ComponentType::FLOAT: {
            auto it = input.float_values.find(component_path);
            if (it != input.float_values.end()) {
                out_state->float_value = it->second;
                out_state->boolean_value = (it->second > 0.5f) ? 1 : 0;
            }
            break;
        }
        case ComponentType::BOOLEAN: {
            auto it = input.boolean_values.find(component_path);
            if (it != input.boolean_values.end()) {
                out_state->boolean_value = it->second ? 1 : 0;
                out_state->float_value = it->second ? 1.0f : 0.0f;
            }
            break;
        }
        case ComponentType::VEC2: {
            auto it = input.vec2_values.find(component_path);
            if (it != input.vec2_values.end()) {
                out_state->x = it->second.x;
                out_state->y = it->second.y;
            }
            break;
        }
    }

    return OX_COMPONENT_AVAILABLE;
}

void SimulatorCore::SetDevicePose(const char* user_path, const OxPose& pose, bool is_active) {
    std::lock_guard<std::mutex> lock(state_mutex_);

    // Find the device by user path (now includes /user/head at device[0])
    for (uint32_t i = 0; i < state_.device_count && i < OX_MAX_DEVICES; i++) {
        if (std::strcmp(state_.devices[i].user_path, user_path) == 0) {
            state_.devices[i].pose = pose;
            // HMD (/user/head) is always active
            state_.devices[i].is_active = (std::strcmp(user_path, "/user/head") == 0) ? 1 : (is_active ? 1 : 0);
            return;
        }
    }
}

void SimulatorCore::SetInputComponent(const char* user_path, const char* component_path, float value) {
    std::lock_guard<std::mutex> lock(state_mutex_);

    // Find the device by user path
    int device_index = -1;
    for (uint32_t i = 0; i < state_.device_count && i < OX_MAX_DEVICES; i++) {
        if (std::strcmp(state_.devices[i].user_path, user_path) == 0) {
            device_index = i;
            break;
        }
    }

    if (device_index < 0) {
        return;
    }

    // Validate that this component exists for this device in the profile
    if (!profile_) {
        return;
    }

    const DeviceDef* device_def = nullptr;
    for (const auto& dev : profile_->devices) {
        if (std::strcmp(dev.user_path, user_path) == 0) {
            device_def = &dev;
            break;
        }
    }

    if (!device_def) {
        return;
    }

    // Check if component exists in device definition
    bool component_exists = false;
    ComponentType comp_type = ComponentType::FLOAT;
    for (const auto& comp : device_def->components) {
        if (std::strcmp(comp.path, component_path) == 0) {
            component_exists = true;
            comp_type = comp.type;
            break;
        }
    }

    if (!component_exists) {
        return;
    }

    DeviceInputState& input = state_.device_inputs[device_index];

    // Set value in dynamic storage
    switch (comp_type) {
        case ComponentType::FLOAT:
            input.float_values[component_path] = value;
            break;
        case ComponentType::BOOLEAN:
            input.boolean_values[component_path] = (value >= 0.5f);
            break;
        case ComponentType::VEC2:
            // For VEC2, this sets just one axis - caller should use proper API
            // For now, we'll just ignore single value sets for VEC2
            break;
    }
}

}  // namespace ox_sim
