#include "gui_window.h"

#define NOMINMAX

#include <GLFW/glfw3.h>

#include <algorithm>
#include <cmath>
#include <iostream>

#include "device_profiles.h"
#include "frame_data.h"
#include "imgui_impl_opengl3.h"
#include "utils.hpp"
#include "vog.h"

namespace ox_sim {

GuiWindow::GuiWindow() = default;
GuiWindow::~GuiWindow() { Stop(); }

bool GuiWindow::Start(SimulatorCore* simulator, const DeviceProfile** device_profile_ptr, bool* api_enabled) {
    if (!simulator) {
        std::cerr << "GuiWindow::Start: Simulator is null" << std::endl;
        return false;
    }
    if (!device_profile_ptr) {
        std::cerr << "GuiWindow::Start: Device profile pointer is null" << std::endl;
        return false;
    }
    if (!api_enabled) {
        std::cerr << "GuiWindow::Start: API enabled pointer is null" << std::endl;
        return false;
    }
    if (window_.IsRunning()) {
        std::cerr << "GuiWindow::Start: GUI already running" << std::endl;
        return false;
    }

    simulator_ = simulator;
    device_profile_ptr_ = device_profile_ptr;
    api_enabled_ = api_enabled;

    if (*device_profile_ptr_) {
        selected_device_type_ = static_cast<int>((*device_profile_ptr_)->type);
    }

    std::cout << "Initializing GUI window..." << std::endl;
    vog::WindowConfig cfg{"ox simulator", 1280, 720};
    return window_.Start(cfg, [this]() { RenderFrame(); });
}

void GuiWindow::Stop() { window_.Stop(); }

// ---------------------------------------------------------------------------
// RenderFrame — pure ImGui widget calls, invoked once per frame by
// vog::Window between NewFrame() and Render().
// ---------------------------------------------------------------------------

void GuiWindow::RenderFrame() {
    const vog::ThemeColors& tc = vog::GetThemeColors();

    ImGuiIO& io = ImGui::GetIO();
    ImVec2 window_size = io.DisplaySize;

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(window_size);

    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                    ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings |
                                    ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::Begin("MainWindow", nullptr, window_flags);
    ImVec2 content_size = ImGui::GetContentRegionAvail();
    const ImGuiStyle& style = ImGui::GetStyle();

    // ========== TOP TOOLBAR STRIP ==========
    const float top_toolbar_h = 48.0f;
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, tc.surface);
    ImGui::BeginChild("TopToolbar", ImVec2(0, top_toolbar_h), false, ImGuiWindowFlags_NoScrollbar);
    {
        const float btn_runtime_w = 190.0f;
        const float btn_api_w = 160.0f;
        const float lbl_device_w = ImGui::CalcTextSize("Device:").x + style.ItemSpacing.x;
        const float combo_device_w = 190.0f;
        const float spacing = style.ItemSpacing.x * 3.0f;
        const float total_w = btn_runtime_w + spacing + btn_api_w + spacing + lbl_device_w + combo_device_w;
        const ImVec2 avail = ImGui::GetContentRegionAvail();

        float start_x = (avail.x - total_w) * 0.5f;
        if (start_x < 0.0f) start_x = 0.0f;
        float center_y = (avail.y - ImGui::GetFrameHeight()) * 0.5f;
        if (center_y < 0.0f) center_y = 0.0f;
        ImGui::SetCursorPos(ImVec2(start_x, center_y));

        if (ImGui::Button("Set as OpenXR Runtime", ImVec2(btn_runtime_w, 0))) {
            ox_sim::utils::SetAsOpenXRRuntime(status_message_);
        }
        vog::widgets::ShowItemTooltip("Register ox simulator as the active OpenXR runtime on this system");

        ImGui::SameLine();

        if (ImGui::Button(ICON_FA_COPY "##copy_runtime_path")) {
            glfwSetClipboardString(window_.GetNativeWindow(), ox_sim::utils::GetRuntimeJsonPath().string().c_str());
            status_message_ = "Copied runtime path to clipboard";
        }
        vog::widgets::ShowItemTooltip(
            "Copy the path to the OpenXR runtime JSON file to clipboard. Set this as the XR_RUNTIME_JSON environment "
            "variable.");

        ImGui::SameLine(0, spacing);

        bool api_on = *api_enabled_;
        if (vog::widgets::ToggleButton("API Server:", &api_on, false)) {
            *api_enabled_ = api_on;
            status_message_ = api_on ? "API Server enabled (port 8765)" : "API Server disabled";
        }
        vog::widgets::ShowItemTooltip("Toggle HTTP API server on port 8765");

        ImGui::SameLine(0, spacing);

        ImGui::AlignTextToFramePadding();
        ImGui::Text("Simulated Device:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(combo_device_w);
        const char* device_names[] = {"Meta Quest 2", "Meta Quest 3", "HTC Vive", "Valve Index", "HTC Vive Tracker"};
        int current_device = selected_device_type_;
        if (vog::widgets::Combo("##DeviceSelect", &current_device, device_names, IM_ARRAYSIZE(device_names))) {
            DeviceType new_type = static_cast<DeviceType>(current_device);
            const DeviceProfile& new_profile = GetDeviceProfile(new_type);
            if (simulator_->SwitchDevice(&new_profile)) {
                selected_device_type_ = current_device;
                *device_profile_ptr_ = &new_profile;
                status_message_ = std::string("Switched to ") + new_profile.name;
            } else {
                status_message_ = "Failed to switch device profile";
            }
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();

    // ========== MAIN AREA: Preview (left) + Sidebar (right) ==========
    const float sidebar_w = 360.0f;
    const float status_bar_h = 30.0f;
    const float preview_w = content_size.x - sidebar_w - style.ItemSpacing.x;
    const float main_area_h = content_size.y - top_toolbar_h - status_bar_h - style.ItemSpacing.y;

    ImGui::SetCursorPosY(top_toolbar_h);

    const float preview_padding = 5.0f;
    ImGui::SetCursorPos(ImVec2(preview_padding, top_toolbar_h));
    ImGui::BeginChild("PreviewArea", ImVec2(preview_w - preview_padding, main_area_h), false,
                      ImGuiWindowFlags_NoScrollbar);
    {
        RenderFramePreview();
    }
    ImGui::EndChild();

    ImGui::SetCursorPos(ImVec2(preview_w + style.ItemSpacing.x, top_toolbar_h));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::BeginChild("Sidebar", ImVec2(sidebar_w, main_area_h), false);
    {
        if (*device_profile_ptr_) {
            const DeviceProfile* profile = *device_profile_ptr_;
            const float inner_w = sidebar_w - style.ScrollbarSize - style.WindowPadding.x * 2.0f;
            for (size_t i = 0; i < profile->devices.size(); i++) {
                if (i > 0) ImGui::Spacing();
                RenderDevicePanel(profile->devices[i], static_cast<int>(i), inner_w);
            }
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleVar();

    // ---- Status bar ----
    ImGui::BeginChild("StatusBar", ImVec2(0, status_bar_h), false, ImGuiWindowFlags_NoScrollbar);
    {
        ImGui::Separator();
        ImGui::Indent(5.0f);
        if (*device_profile_ptr_) {
            const DeviceProfile* p = *device_profile_ptr_;
            ImGui::Text("Display: %dx%d @ %.0f Hz  |  %s", p->display_width, p->display_height, p->refresh_rate,
                        status_message_.c_str());
        } else {
            ImGui::Text("%s", status_message_.c_str());
        }
        ImGui::Unindent(5.0f);
    }
    ImGui::EndChild();

    ImGui::End();
}

void GuiWindow::RenderFramePreview() {
    using vog::widgets::Combo;

    UpdateFrameTextures();

    const ImVec2 region = ImGui::GetContentRegionAvail();
    const float toolbar_h = 30.0f;
    const float content_h = region.y - toolbar_h;
    const bool has_image = preview_textures_valid_ && preview_width_ > 0 && preview_height_ > 0;

    ImGui::BeginChild("PreviewToolbar", ImVec2(0, toolbar_h), false, ImGuiWindowFlags_NoScrollbar);
    {
        const float combo_w = 80.0f;
        const float label_w = ImGui::CalcTextSize("View:").x + ImGui::GetStyle().ItemSpacing.x;
        float cursor_x = ImGui::GetContentRegionAvail().x - combo_w - label_w;
        if (cursor_x < 0.0f) cursor_x = 0.0f;
        ImGui::SetCursorPosX(cursor_x);
        ImGui::AlignTextToFramePadding();
        ImGui::Text("View:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(combo_w);
        const char* eye_names[] = {"Left", "Right", "Both"};
        Combo("##EyeSelect", &preview_eye_selection_, eye_names, 3);
    }
    ImGui::EndChild();

    ImGui::BeginChild("PreviewContent", ImVec2(0, content_h), false, ImGuiWindowFlags_NoScrollbar);
    {
        const ImVec2 avail = ImGui::GetContentRegionAvail();

        if (!has_image) {
            const char* msg = "No image received";
            ImVec2 ts = ImGui::CalcTextSize(msg);
            ImGui::SetCursorPos(ImVec2((avail.x - ts.x) * 0.5f, (avail.y - ts.y) * 0.5f));
            ImGui::Text("%s", msg);
        } else if (preview_eye_selection_ == 2) {
            const float aspect = (float)preview_width_ / (float)preview_height_;
            float w_each = avail.x * 0.5f;
            float h_each = w_each / aspect;
            if (h_each > avail.y) {
                h_each = avail.y;
                w_each = h_each * aspect;
            }
            const float y_off = (avail.y - h_each) * 0.5f;
            const float left_x = std::max(0.0f, avail.x * 0.5f - w_each);
            const float right_x = left_x + w_each;
            ImGui::SetCursorPos(ImVec2(left_x, y_off));
            if (preview_textures_[0]) {
                ImGui::Image((ImTextureID)(intptr_t)preview_textures_[0], ImVec2(w_each, h_each), ImVec2(0, 1),
                             ImVec2(1, 0));
            } else {
                ImGui::Dummy(ImVec2(w_each, h_each));
            }
            ImGui::SetCursorPos(ImVec2(right_x, y_off));
            if (preview_textures_[1]) {
                ImGui::Image((ImTextureID)(intptr_t)preview_textures_[1], ImVec2(w_each, h_each), ImVec2(0, 1),
                             ImVec2(1, 0));
            } else {
                ImGui::Dummy(ImVec2(w_each, h_each));
            }
        } else {
            const int eye = (preview_eye_selection_ == 1) ? 1 : 0;
            const char* no_msg = (eye == 1) ? "No image received (right eye)" : "No image received (left eye)";
            if (preview_textures_[eye]) {
                const float aspect = (float)preview_width_ / (float)preview_height_;
                float img_w = avail.x;
                float img_h = img_w / aspect;
                if (img_h > avail.y) {
                    img_h = avail.y;
                    img_w = img_h * aspect;
                }
                const float x_off = (avail.x - img_w) * 0.5f;
                const float y_off = (avail.y - img_h) * 0.5f;
                ImGui::SetCursorPos(ImVec2(x_off, y_off));
                ImGui::Image((ImTextureID)(intptr_t)preview_textures_[eye], ImVec2(img_w, img_h), ImVec2(0, 1),
                             ImVec2(1, 0));
            } else {
                ImVec2 ts = ImGui::CalcTextSize(no_msg);
                ImGui::SetCursorPos(ImVec2((avail.x - ts.x) * 0.5f, (avail.y - ts.y) * 0.5f));
                ImGui::Text("%s", no_msg);
            }
        }
    }
    ImGui::EndChild();
}

void GuiWindow::UpdateFrameTextures() {
    FrameData* frame_data = GetFrameData();
    if (!frame_data) return;
    if (!frame_data->has_new_frame.load(std::memory_order_acquire)) return;

    std::lock_guard<std::mutex> lock(frame_data->mutex);
    uint32_t w = frame_data->width;
    uint32_t h = frame_data->height;
    if (w == 0 || h == 0) return;

    for (int eye = 0; eye < 2; ++eye) {
        if (!frame_data->pixel_data[eye]) continue;
        size_t expected_size = w * h * 4;
        if (frame_data->data_size[eye] != expected_size) continue;

        if (!preview_textures_[eye]) {
            glGenTextures(1, &preview_textures_[eye]);
            glBindTexture(GL_TEXTURE_2D, preview_textures_[eye]);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            std::cout << "[GUI] Created OpenGL texture " << preview_textures_[eye] << " for eye " << eye << std::endl;
        }
        glBindTexture(GL_TEXTURE_2D, preview_textures_[eye]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, frame_data->pixel_data[eye]);
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    preview_width_ = w;
    preview_height_ = h;
    preview_textures_valid_ = true;
    frame_data->has_new_frame.store(false, std::memory_order_release);
}

void GuiWindow::RenderDevicePanel(const DeviceDef& device, int device_index, float panel_width) {
    using vog::widgets::ShowItemTooltip;
    const vog::ThemeColors& tc = vog::GetThemeColors();
    ImGui::PushID(device_index);

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    const float pad = 8.0f;
    const float rounding = 4.0f;
    const ImVec2 panel_tl = ImGui::GetCursorScreenPos();

    ImGui::SetCursorScreenPos(ImVec2(panel_tl.x + pad, panel_tl.y + pad));
    ImGui::BeginGroup();
    ImGui::PushItemWidth(panel_width - pad * 2.0f);

    std::string device_label = std::string(device.role);
    ImGui::TextColored(tc.accent, "%s", device_label.c_str());
    ImGui::SameLine();
    ImGui::TextColored(tc.text_muted, "(%s)", device.user_path);
    ImGui::Separator();

    bool is_active = false;
    OxPose pose = {{0, 0, 0}, {0, 0, 0, 1}};
    simulator_->GetDevicePose(device.user_path, &pose, &is_active);

    if (!device.always_active) {
        bool active_toggle = is_active;
        if (ImGui::Checkbox("Active", &active_toggle)) {
            simulator_->SetDevicePose(device.user_path, pose, active_toggle);
        }
        ShowItemTooltip("Enable/disable device tracking");
    } else {
        ImGui::TextColored(tc.positive, "Active: Always On");
        ShowItemTooltip("This device is always active");
    }

    ImGui::Spacing();
    ImGui::Separator();

    ImGui::TextColored(tc.warning, "Pose");
    ImGui::Spacing();

    ImGui::Text("Position:");
    float pos[3] = {pose.position.x, pose.position.y, pose.position.z};
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::DragFloat3("##Position", pos, 0.01f, -10.0f, 10.0f, "%.2f")) {
        pose.position = {pos[0], pos[1], pos[2]};
        simulator_->SetDevicePose(device.user_path, pose, is_active);
    }

    ImGui::Spacing();
    ImGui::Text("Rotation (Euler XYZ):");

    float x = pose.orientation.x, y = pose.orientation.y;
    float z = pose.orientation.z, w = pose.orientation.w;

    float sinr_cosp = 2.0f * (w * x + y * z);
    float cosr_cosp = 1.0f - 2.0f * (x * x + y * y);
    float roll = std::atan2(sinr_cosp, cosr_cosp);

    float sinp = 2.0f * (w * y - z * x);
    float pitch = std::abs(sinp) >= 1.0f ? std::copysign(3.14159265f / 2.0f, sinp) : std::asin(sinp);

    float siny_cosp = 2.0f * (w * z + x * y);
    float cosy_cosp = 1.0f - 2.0f * (y * y + z * z);
    float yaw = std::atan2(siny_cosp, cosy_cosp);

    float euler[3] = {roll * 57.2958f, pitch * 57.2958f, yaw * 57.2958f};
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::DragFloat3("##Orientation", euler, 1.0f, -180.0f, 180.0f, "%.1f\xc2\xb0")) {
        float cy = std::cos(euler[2] * 0.0174533f * 0.5f);
        float sy = std::sin(euler[2] * 0.0174533f * 0.5f);
        float cp = std::cos(euler[1] * 0.0174533f * 0.5f);
        float sp = std::sin(euler[1] * 0.0174533f * 0.5f);
        float cr = std::cos(euler[0] * 0.0174533f * 0.5f);
        float sr = std::sin(euler[0] * 0.0174533f * 0.5f);

        pose.orientation.w = cr * cp * cy + sr * sp * sy;
        pose.orientation.x = sr * cp * cy - cr * sp * sy;
        pose.orientation.y = cr * sp * cy + sr * cp * sy;
        pose.orientation.z = cr * cp * sy - sr * sp * cy;
        simulator_->SetDevicePose(device.user_path, pose, is_active);
    }

    ImGui::Spacing();
    if (ImGui::Button("Reset Pose", ImVec2(-FLT_MIN, 0))) {
        simulator_->SetDevicePose(device.user_path, device.default_pose, is_active);
    }

    if (!device.components.empty()) {
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextColored(tc.warning, "Input Components");
        ImGui::Spacing();
        for (const auto& component : device.components) {
            RenderComponentControl(device, component, device_index);
        }
    }

    ImGui::PopItemWidth();
    ImGui::EndGroup();

    const ImVec2 group_br = ImGui::GetItemRectMax();
    const ImVec2 panel_br = ImVec2(panel_tl.x + panel_width, group_br.y + pad);
    draw_list->AddRect(panel_tl, panel_br, ImGui::ColorConvertFloat4ToU32(tc.border), rounding);

    ImGui::SetCursorScreenPos(ImVec2(panel_tl.x, panel_br.y));
    ImGui::Dummy(ImVec2(panel_width, 0.0f));

    ImGui::PopID();
}

void GuiWindow::RenderComponentControl(const DeviceDef& device, const ComponentDef& component, int device_index) {
    ImGui::PushID(component.path);

    switch (component.type) {
        case ComponentType::BOOLEAN: {
            bool value = false;
            simulator_->GetInputStateBoolean(device.user_path, component.path, &value);
            if (ImGui::Checkbox(component.description, &value)) {
                simulator_->SetInputStateBoolean(device.user_path, component.path, value);
            }
            break;
        }
        case ComponentType::FLOAT: {
            float value = 0.0f;
            simulator_->GetInputStateFloat(device.user_path, component.path, &value);
            ImGui::Text("%s:", component.description);
            ImGui::SetNextItemWidth(-1);
            if (ImGui::SliderFloat("##value", &value, 0.0f, 1.0f, "%.2f")) {
                simulator_->SetInputStateFloat(device.user_path, component.path, value);
            }
            break;
        }
        case ComponentType::VEC2: {
            OxVector2f value = {0.0f, 0.0f};
            simulator_->GetInputStateVec2(device.user_path, component.path, &value);
            ImGui::Text("%s:", component.description);
            float vec2[2] = {value.x, value.y};
            ImGui::SetNextItemWidth(-1);
            if (ImGui::SliderFloat2("##vec2", vec2, -1.0f, 1.0f, "%.2f")) {
                value.x = vec2[0];
                value.y = vec2[1];
                simulator_->SetInputStateVec2(device.user_path, component.path, value);
            }
            break;
        }
    }

    ImGui::PopID();
    ImGui::Spacing();
}

}  // namespace ox_sim
