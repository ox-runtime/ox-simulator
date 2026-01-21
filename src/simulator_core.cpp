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
    {"/input/trigger/value", {ComponentMapping::FLOAT, offsetof(DeviceState::InputState, trigger_value)}},
    {"/input/trigger/click", {ComponentMapping::BOOLEAN, offsetof(DeviceState::InputState, trigger_click)}},
    {"/input/trigger/touch", {ComponentMapping::BOOLEAN, offsetof(DeviceState::InputState, trigger_touch)}},
    {"/input/squeeze/value", {ComponentMapping::FLOAT, offsetof(DeviceState::InputState, grip_value)}},
    {"/input/squeeze/click", {ComponentMapping::BOOLEAN, offsetof(DeviceState::InputState, grip_click)}},
    {"/input/thumbstick/x", {ComponentMapping::FLOAT, offsetof(DeviceState::InputState, thumbstick_x)}},
    {"/input/thumbstick/y", {ComponentMapping::FLOAT, offsetof(DeviceState::InputState, thumbstick_y)}},
    {"/input/thumbstick/click", {ComponentMapping::BOOLEAN, offsetof(DeviceState::InputState, thumbstick_click)}},
    {"/input/thumbstick/touch", {ComponentMapping::BOOLEAN, offsetof(DeviceState::InputState, thumbstick_touch)}},
    {"/input/a/click", {ComponentMapping::BOOLEAN, offsetof(DeviceState::InputState, button_a_click)}},
    {"/input/x/click", {ComponentMapping::BOOLEAN, offsetof(DeviceState::InputState, button_a_click)}},
    {"/input/a/touch", {ComponentMapping::BOOLEAN, offsetof(DeviceState::InputState, button_a_touch)}},
    {"/input/x/touch", {ComponentMapping::BOOLEAN, offsetof(DeviceState::InputState, button_a_touch)}},
    {"/input/b/click", {ComponentMapping::BOOLEAN, offsetof(DeviceState::InputState, button_b_click)}},
    {"/input/y/click", {ComponentMapping::BOOLEAN, offsetof(DeviceState::InputState, button_b_click)}},
    {"/input/b/touch", {ComponentMapping::BOOLEAN, offsetof(DeviceState::InputState, button_b_touch)}},
    {"/input/y/touch", {ComponentMapping::BOOLEAN, offsetof(DeviceState::InputState, button_b_touch)}},
    {"/input/menu/click", {ComponentMapping::BOOLEAN, offsetof(DeviceState::InputState, menu_click)}},
    {"/input/system/click", {ComponentMapping::BOOLEAN, offsetof(DeviceState::InputState, menu_click)}},
};

SimulatorCore::SimulatorCore() : profile_(nullptr), state_{} {
    // Default: HMD connected, no devices initially
    // Device[0] will be set up as /user/head during Initialize()
    state_.device_count = 0;
}

SimulatorCore::~SimulatorCore() { Shutdown(); }

bool SimulatorCore::Initialize(const DeviceProfile* profile) {
    if (!profile) {
        return false;
    }

    std::lock_guard<std::mutex> lock(state_mutex_);
    profile_ = profile;

    // Initialize devices: HMD + 2 hand controllers
    state_.device_count = 3;

    // HMD as device[0] - /user/head
    std::strncpy(state_.devices[0].user_path, "/user/head", sizeof(state_.devices[0].user_path) - 1);
    state_.devices[0].user_path[sizeof(state_.devices[0].user_path) - 1] = '\0';
    state_.devices[0].is_active = 1;  // HMD is always active
    state_.devices[0].pose.position = {0.0f, 1.6f, 0.0f};
    state_.devices[0].pose.orientation = {0.0f, 0.0f, 0.0f, 1.0f};

    // Left hand controller as device[1]
    std::strncpy(state_.devices[1].user_path, "/user/hand/left", sizeof(state_.devices[1].user_path) - 1);
    state_.devices[1].user_path[sizeof(state_.devices[1].user_path) - 1] = '\0';
    state_.devices[1].is_active = 0;
    state_.devices[1].pose.position = {-0.2f, 1.4f, -0.3f};
    state_.devices[1].pose.orientation = {0.0f, 0.0f, 0.0f, 1.0f};

    // Right hand controller as device[2]
    std::strncpy(state_.devices[2].user_path, "/user/hand/right", sizeof(state_.devices[2].user_path) - 1);
    state_.devices[2].user_path[sizeof(state_.devices[2].user_path) - 1] = '\0';
    state_.devices[2].is_active = 0;
    state_.devices[2].pose.position = {0.2f, 1.4f, -0.3f};
    state_.devices[2].pose.orientation = {0.0f, 0.0f, 0.0f, 1.0f};

    return true;
}

void SimulatorCore::Shutdown() {
    std::lock_guard<std::mutex> lock(state_mutex_);
    profile_ = nullptr;
}

void SimulatorCore::GetHMDPose(OxPose* out_pose) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    // HMD is now device[0] with /user/head path
    *out_pose = state_.devices[0].pose;
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

    const DeviceState::InputState& input = state_.device_inputs[device_index];

    memset(out_state, 0, sizeof(OxInputComponentState));

    // Special case for 2D thumbstick
    if (std::strcmp(component_path, "/input/thumbstick") == 0) {
        out_state->x = input.thumbstick_x;
        out_state->y = input.thumbstick_y;
        return OX_COMPONENT_AVAILABLE;
    }

    // Look up component in map
    auto it = g_component_map.find(component_path);
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
    // HMD is now device[0] with /user/head path
    state_.devices[0].pose = pose;
    state_.devices[0].is_active = 1;  // HMD is always active
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

    DeviceState::InputState& input = state_.device_inputs[device_index];

    // Look up component in map
    auto it = g_component_map.find(component_path);
    if (it == g_component_map.end()) {
        return;
    }

    const ComponentMapping& mapping = it->second;
    char* base = reinterpret_cast<char*>(&input);

    if (mapping.type == ComponentMapping::FLOAT) {
        *reinterpret_cast<float*>(base + mapping.offset) = value;
    } else if (mapping.type == ComponentMapping::BOOLEAN) {
        *reinterpret_cast<bool*>(base + mapping.offset) = (value >= 0.5f);
    }
}

}  // namespace ox_sim
