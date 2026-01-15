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

    // Default: HMD connected, controllers not connected
    state_.hmd_connected.store(true);
    state_.left_controller_connected.store(false);
    state_.right_controller_connected.store(false);
}

SimulatorCore::~SimulatorCore() { Shutdown(); }

bool SimulatorCore::Initialize(const DeviceProfile* profile) {
    if (!profile) {
        return false;
    }

    std::lock_guard<std::mutex> lock(state_mutex_);
    profile_ = profile;

    // Initialize controller poses (default positions)
    state_.controllers[0].is_active = 0;
    state_.controllers[0].pose.position = {-0.2f, 1.4f, -0.3f};  // Left controller
    state_.controllers[0].pose.orientation = {0.0f, 0.0f, 0.0f, 1.0f};

    state_.controllers[1].is_active = 0;
    state_.controllers[1].pose.position = {0.2f, 1.4f, -0.3f};  // Right controller
    state_.controllers[1].pose.orientation = {0.0f, 0.0f, 0.0f, 1.0f};

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

void SimulatorCore::GetControllerState(uint32_t controller_index, OxControllerState* out_state) {
    if (controller_index >= 2) {
        out_state->is_active = 0;
        return;
    }

    std::lock_guard<std::mutex> lock(state_mutex_);
    *out_state = state_.controllers[controller_index];
}

bool SimulatorCore::ParseComponentPath(const char* component_path, std::string& component_name) {
    // Component paths look like: /user/hand/left/input/trigger/value
    // We need to extract the component part: trigger/value

    std::string path(component_path);

    // Find "/input/" and extract everything after it
    size_t input_pos = path.find("/input/");
    if (input_pos == std::string::npos) {
        return false;
    }

    component_name = path.substr(input_pos + 7);  // Skip "/input/"
    return true;
}

OxComponentResult SimulatorCore::GetInputComponentState(uint32_t controller_index, const char* component_path,
                                                        OxInputComponentState* out_state) {
    if (controller_index >= 2) {
        return OX_COMPONENT_UNAVAILABLE;
    }

    std::lock_guard<std::mutex> lock(state_mutex_);

    const DeviceState::InputState& input = (controller_index == 0) ? state_.left_input : state_.right_input;

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
        out_state->float_value = *reinterpret_cast<const float*>(base + mapping.offset);
    } else if (mapping.type == ComponentMapping::BOOLEAN) {
        out_state->boolean_value = *reinterpret_cast<const bool*>(base + mapping.offset) ? 1 : 0;
    }

    return OX_COMPONENT_AVAILABLE;
}

void SimulatorCore::SetHMDPose(const OxPose& pose) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    state_.hmd_pose = pose;
}

void SimulatorCore::SetControllerPose(uint32_t controller_index, const OxPose& pose, bool is_active) {
    if (controller_index >= 2) return;

    std::lock_guard<std::mutex> lock(state_mutex_);
    state_.controllers[controller_index].pose = pose;
    state_.controllers[controller_index].is_active = is_active ? 1 : 0;

    if (controller_index == 0) {
        state_.left_controller_connected.store(is_active);
    } else {
        state_.right_controller_connected.store(is_active);
    }
}

void SimulatorCore::SetInputComponent(uint32_t controller_index, const char* component_path, float value,
                                      bool boolean_value) {
    if (controller_index >= 2) return;

    std::lock_guard<std::mutex> lock(state_mutex_);

    DeviceState::InputState& input = (controller_index == 0) ? state_.left_input : state_.right_input;

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
