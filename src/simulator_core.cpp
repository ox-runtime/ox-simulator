#include "simulator_core.h"

#include <cmath>
#include <cstring>
#include <unordered_map>

namespace ox_sim {

// Component mapping structures
struct ComponentMapping {
    enum Type { FLOAT, BOOLEAN, VEC2 } type;
    size_t offset;
};

static const std::unordered_map<std::string, ComponentMapping> g_component_map = {
    {"trigger/value", {ComponentMapping::FLOAT, offsetof(DeviceState::InputState, trigger_value)}},
    {"trigger/click", {ComponentMapping::BOOLEAN, offsetof(DeviceState::InputState, trigger_click)}},
    {"trigger/touch", {ComponentMapping::BOOLEAN, offsetof(DeviceState::InputState, trigger_touch)}},
    {"squeeze/value", {ComponentMapping::FLOAT, offsetof(DeviceState::InputState, grip_value)}},
    {"squeeze/click", {ComponentMapping::BOOLEAN, offsetof(DeviceState::InputState, grip_click)}},
    {"thumbstick/x", {ComponentMapping::FLOAT, offsetof(DeviceState::InputState, thumbstick_x)}},
    {"thumbstick/y", {ComponentMapping::FLOAT, offsetof(DeviceState::InputState, thumbstick_y)}},
    {"thumbstick/click", {ComponentMapping::BOOLEAN, offsetof(DeviceState::InputState, thumbstick_click)}},
    {"thumbstick/touch", {ComponentMapping::BOOLEAN, offsetof(DeviceState::InputState, thumbstick_touch)}},
    {"a/click", {ComponentMapping::BOOLEAN, offsetof(DeviceState::InputState, button_a_click)}},
    {"x/click", {ComponentMapping::BOOLEAN, offsetof(DeviceState::InputState, button_a_click)}},
    {"a/touch", {ComponentMapping::BOOLEAN, offsetof(DeviceState::InputState, button_a_touch)}},
    {"x/touch", {ComponentMapping::BOOLEAN, offsetof(DeviceState::InputState, button_a_touch)}},
    {"b/click", {ComponentMapping::BOOLEAN, offsetof(DeviceState::InputState, button_b_click)}},
    {"y/click", {ComponentMapping::BOOLEAN, offsetof(DeviceState::InputState, button_b_click)}},
    {"b/touch", {ComponentMapping::BOOLEAN, offsetof(DeviceState::InputState, button_b_touch)}},
    {"y/touch", {ComponentMapping::BOOLEAN, offsetof(DeviceState::InputState, button_b_touch)}},
    {"menu/click", {ComponentMapping::BOOLEAN, offsetof(DeviceState::InputState, menu_click)}},
    {"system/click", {ComponentMapping::BOOLEAN, offsetof(DeviceState::InputState, menu_click)}},
};

SimulatorCore::SimulatorCore() : profile_(nullptr) {
    memset(&state_, 0, sizeof(state_));

    // Default HMD pose: standing height
    state_.hmd_pose.position = {0.0f, 1.6f, 0.0f};
    state_.hmd_pose.orientation = {0.0f, 0.0f, 0.0f, 1.0f};

    // Default: HMD connected, no devices initially
    state_.hmd_connected.store(true);
    state_.device_count = 0;
}

SimulatorCore::~SimulatorCore() { Shutdown(); }

bool SimulatorCore::Initialize(const DeviceProfile* profile) {
    if (!profile) {
        return false;
    }

    std::lock_guard<std::mutex> lock(state_mutex_);
    profile_ = profile;

    // Initialize devices (2 hand controllers by default)
    state_.device_count = 2;

    // Left hand controller
    std::strncpy(state_.devices[0].user_path, "/user/hand/left", sizeof(state_.devices[0].user_path) - 1);
    state_.devices[0].user_path[sizeof(state_.devices[0].user_path) - 1] = '\0';
    state_.devices[0].is_active = 0;
    state_.devices[0].pose.position = {-0.2f, 1.4f, -0.3f};
    state_.devices[0].pose.orientation = {0.0f, 0.0f, 0.0f, 1.0f};

    // Right hand controller
    std::strncpy(state_.devices[1].user_path, "/user/hand/right", sizeof(state_.devices[1].user_path) - 1);
    state_.devices[1].user_path[sizeof(state_.devices[1].user_path) - 1] = '\0';
    state_.devices[1].is_active = 0;
    state_.devices[1].pose.position = {0.2f, 1.4f, -0.3f};
    state_.devices[1].pose.orientation = {0.0f, 0.0f, 0.0f, 1.0f};

    return true;
}

void SimulatorCore::Shutdown() {
    std::lock_guard<std::mutex> lock(state_mutex_);
    profile_ = nullptr;
}

void SimulatorCore::GetHMDPose(OxPose* out_pose) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    *out_pose = state_.hmd_pose;
}

void SimulatorCore::GetAllDevices(OxDeviceState* out_states, uint32_t* out_count) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    *out_count = state_.device_count;
    for (uint32_t i = 0; i < state_.device_count && i < OX_MAX_DEVICES; i++) {
        out_states[i] = state_.devices[i];
    }
}

bool SimulatorCore::ParseComponentPath(const char* component_path, std::string& component_name) {
    // Component path is the full OpenXR path: "/user/hand/left/input/trigger/value"
    // Extract just the component part after /input/: "trigger/value"

    std::string path(component_path);

    // Find the "/input/" portion in the full path
    size_t input_pos = path.find("/input/");
    if (input_pos == std::string::npos) {
        return false;
    }

    component_name = path.substr(input_pos + 7);  // Skip "/input/"
    return true;
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

    const DeviceState::InputState& input = state_.device_inputs[device_index];

    std::string component;
    if (!ParseComponentPath(component_path, component)) {
        return OX_COMPONENT_UNAVAILABLE;
    }

    memset(out_state, 0, sizeof(OxInputComponentState));

    // Special case for 2D thumbstick
    if (component == "thumbstick") {
        out_state->x = input.thumbstick_x;
        out_state->y = input.thumbstick_y;
        return OX_COMPONENT_AVAILABLE;
    }

    // Look up component in map
    auto it = g_component_map.find(component);
    if (it == g_component_map.end()) {
        return OX_COMPONENT_UNAVAILABLE;
    }

    const ComponentMapping& mapping = it->second;
    const char* base = reinterpret_cast<const char*>(&input);

    if (mapping.type == ComponentMapping::FLOAT) {
        float val = *reinterpret_cast<const float*>(base + mapping.offset);
        out_state->float_value = val;
        out_state->boolean_value = (val > 0.5f) ? 1 : 0;
    } else if (mapping.type == ComponentMapping::BOOLEAN) {
        bool val = *reinterpret_cast<const bool*>(base + mapping.offset);
        out_state->boolean_value = val ? 1 : 0;
        out_state->float_value = val ? 1.0f : 0.0f;
    }

    return OX_COMPONENT_AVAILABLE;
}

void SimulatorCore::SetHMDPose(const OxPose& pose) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    state_.hmd_pose = pose;
}

void SimulatorCore::SetDevicePose(const char* user_path, const OxPose& pose, bool is_active) {
    std::lock_guard<std::mutex> lock(state_mutex_);

    // Find the device by user path
    for (uint32_t i = 0; i < state_.device_count && i < OX_MAX_DEVICES; i++) {
        if (std::strcmp(state_.devices[i].user_path, user_path) == 0) {
            state_.devices[i].pose = pose;
            state_.devices[i].is_active = is_active ? 1 : 0;
            return;
        }
    }
}

void SimulatorCore::SetInputComponent(const char* user_path, const char* component_path, float value,
                                      bool boolean_value) {
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

    DeviceState::InputState& input = state_.device_inputs[device_index];

    std::string component;
    if (!ParseComponentPath(component_path, component)) {
        return;
    }

    // Look up component in map
    auto it = g_component_map.find(component);
    if (it == g_component_map.end()) {
        return;
    }

    const ComponentMapping& mapping = it->second;
    char* base = reinterpret_cast<char*>(&input);

    if (mapping.type == ComponentMapping::FLOAT) {
        *reinterpret_cast<float*>(base + mapping.offset) = value;
    } else if (mapping.type == ComponentMapping::BOOLEAN) {
        *reinterpret_cast<bool*>(base + mapping.offset) = boolean_value;
    }
}

}  // namespace ox_sim
