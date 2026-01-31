#include "gui_window.h"

#include <chrono>
#include <cmath>
#include <iostream>
#include <thread>

// GLFW and OpenGL
#include <GLFW/glfw3.h>

// Dear ImGui
#include "device_profiles.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

namespace ox_sim {

// GLFW error callback
static void glfw_error_callback(int error, const char* description) {
    std::cerr << "GLFW Error " << error << ": " << description << std::endl;
}

GuiWindow::GuiWindow()
    : simulator_(nullptr),
      device_profile_ptr_(nullptr),
      api_enabled_(nullptr),
      running_(false),
      should_stop_(false),
      window_(nullptr),
      selected_device_type_(0),
      status_message_("Ready") {}

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

    if (running_) {
        std::cerr << "GuiWindow::Start: GUI already running" << std::endl;
        return false;
    }

    simulator_ = simulator;
    device_profile_ptr_ = device_profile_ptr;
    api_enabled_ = api_enabled;
    should_stop_ = false;

    // Initialize selected device type based on current profile
    if (*device_profile_ptr_) {
        selected_device_type_ = static_cast<int>((*device_profile_ptr_)->type);
    }

    std::cout << "Initializing GUI window..." << std::endl;

    running_ = true;

    // Start rendering thread
#if defined(__APPLE__)
    // macOS: MUST run render loop on main thread
    std::cout << "Starting GUI render loop on main thread (macOS)" << std::endl;
    RenderLoop();
    return true;
#else
   try {
        render_thread_ = std::thread(&GuiWindow::RenderLoop, this);
        std::cout << "GUI window started successfully" << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "GuiWindow::Start: Exception: " << e.what() << std::endl;
        running_ = false;
        return false;
    }
#endif
}

void GuiWindow::Stop() {
    if (!running_) {
        return;
    }

    std::cout << "Stopping GUI window..." << std::endl;

    should_stop_ = true;

    if (render_thread_.joinable()) {
        render_thread_.join();
    }

    // Graphics cleanup is now handled in the render thread
    running_ = false;
    std::cout << "GUI window stopped" << std::endl;
}

void GuiWindow::RenderLoop() {
    std::cout << "GUI render thread starting..." << std::endl;

    if (!InitializeGraphics()) {
        std::cerr << "Failed to initialize graphics" << std::endl;
        return;
    }

    if (!window_) {
        std::cerr << "GUI render thread: window is null!" << std::endl;
        return;
    }

    std::cout << "GUI render loop ready - entering main loop" << std::endl;

    // Main loop
    while (!should_stop_ && !glfwWindowShouldClose(window_)) {
        glfwPollEvents();

        // Small sleep to avoid burning CPU
        std::this_thread::sleep_for(std::chrono::milliseconds(16));  // ~60 FPS

        RenderFrame();
    }

    std::cout << "GUI render thread exiting" << std::endl;

    // Cleanup graphics resources on this thread
    CleanupGraphics();
}

bool GuiWindow::InitializeGraphics() {
    // Setup GLFW error callback
    glfwSetErrorCallback(glfw_error_callback);

    // Initialize GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return false;
    }

    // Decide GL+GLSL versions
#if defined(__APPLE__)
    // GL 3.2 + GLSL 150 (macOS)
    const char* glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // Required on Mac
#else
    // GL 3.0 + GLSL 130 (Windows/Linux)
    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#endif

    // Create window with graphics context
    window_ = glfwCreateWindow(1280, 720, "ox Simulator Control Panel", nullptr, nullptr);
    if (window_ == nullptr) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(window_);
    glfwSwapInterval(1);  // Enable vsync

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends
    // Note: ImGui_ImplOpenGL3_Init will call imgl3wInit() to load OpenGL functions
    ImGui_ImplGlfw_InitForOpenGL(window_, true);
    if (!ImGui_ImplOpenGL3_Init(glsl_version)) {
        std::cerr << "Failed to initialize ImGui OpenGL3 backend" << std::endl;
        ImGui::DestroyContext();
        glfwDestroyWindow(window_);
        window_ = nullptr;
        glfwTerminate();
        return false;
    }

    // Log OpenGL info while context is still current
    const GLubyte* version = glGetString(GL_VERSION);
    const GLubyte* renderer = glGetString(GL_RENDERER);
    std::cout << "Graphics initialized successfully" << std::endl;
    std::cout << "OpenGL Version: " << (version ? (const char*)version : "Unknown") << std::endl;
    std::cout << "OpenGL Renderer: " << (renderer ? (const char*)renderer : "Unknown") << std::endl;
    std::cout << "GLSL Version: " << glsl_version << std::endl;

    // Context remains current for the render thread
    return true;
}

void GuiWindow::CleanupGraphics() {
    if (window_) {
        // Cleanup ImGui
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();

        // Cleanup GLFW
        glfwDestroyWindow(window_);
        window_ = nullptr;

        glfwTerminate();
    }
}

void GuiWindow::RenderFrame() {
    if (!window_) {
        return;
    }

    // Start the Dear ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // Get window size and use full window (no separate ImGui window)
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 window_size = io.DisplaySize;

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(window_size);

    // Fullscreen window with no decorations
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                    ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings |
                                    ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::Begin("MainWindow", nullptr, window_flags);

    const ImVec2 content_size = ImGui::GetContentRegionAvail();

    // ========== TOP TOOLBAR (Fixed Height) ==========
    const float toolbar_height = 120.0f;
    ImGui::BeginChild("Toolbar", ImVec2(0, toolbar_height), true, ImGuiWindowFlags_NoScrollbar);

    // Header
    ImGui::PushFont(ImGui::GetFont());
    ImGui::TextColored(ImVec4(0.3f, 0.7f, 1.0f, 1.0f), "ox Simulator - Device Control Panel");
    ImGui::PopFont();
    ImGui::Separator();

    // First row: API and Device Selection
    ImGui::Columns(2, "TopRow", false);
    ImGui::SetColumnWidth(0, content_size.x * 0.5f);

    // API Toggle (Left Column)
    ImGui::Text("HTTP API:");
    ImGui::SameLine();
    bool api_enabled = *api_enabled_;
    if (ImGui::Checkbox("Enabled", &api_enabled)) {
        *api_enabled_ = api_enabled;
        status_message_ = api_enabled ? "API enabled (Note: Server cannot be dynamically stopped)"
                                      : "API disabled (Note: Server will remain active)";
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Toggle HTTP API state (Note: Server restart required for changes to take effect)");
    }
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "(Port: 8765)");

    ImGui::NextColumn();

    // Device Selection (Right Column)
    ImGui::Text("Device Profile:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(250);
    const char* device_names[] = {"Meta Quest 2", "Meta Quest 3", "HTC Vive", "Valve Index", "HTC Vive Tracker"};
    int current_device = selected_device_type_;
    if (ImGui::Combo("##DeviceSelect", &current_device, device_names, IM_ARRAYSIZE(device_names))) {
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

    ImGui::Columns(1);
    ImGui::Separator();

    // Second row: Device Information and Status
    if (*device_profile_ptr_) {
        const DeviceProfile* profile = *device_profile_ptr_;
        ImGui::TextColored(ImVec4(0.7f, 0.9f, 0.7f, 1.0f), "Device: %s", profile->name);
        ImGui::SameLine(300);
        ImGui::Text("Manufacturer: %s", profile->manufacturer);
        ImGui::SameLine(600);
        ImGui::Text("Display: %dx%d @ %.0f Hz", profile->display_width, profile->display_height, profile->refresh_rate);
    }

    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Status: %s", status_message_.c_str());

    ImGui::EndChild();

    // ========== DEVICE PANELS (Tiled Layout) ==========
    if (*device_profile_ptr_) {
        const DeviceProfile* profile = *device_profile_ptr_;
        const size_t device_count = profile->devices.size();

        if (device_count > 0) {
            // Calculate grid layout based on device count
            int cols = 1;
            if (device_count >= 4) cols = 2;
            if (device_count >= 6) cols = 3;

            const float spacing = 10.0f;
            const float available_height = content_size.y - toolbar_height - spacing * 2;

            // Use ImGui table for proper grid layout
            ImGuiTableFlags table_flags = ImGuiTableFlags_None;
            if (ImGui::BeginTable("DeviceTable", cols, table_flags)) {
                // Set column widths
                float col_width = (content_size.x - spacing * (cols + 1)) / cols;
                for (int c = 0; c < cols; c++) {
                    ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, col_width);
                }

                for (size_t i = 0; i < device_count; i++) {
                    const int col = i % cols;
                    const int row = i / cols;

                    if (col == 0) {
                        ImGui::TableNextRow();
                    }

                    ImGui::TableSetColumnIndex(col);

                    // Add some padding around each panel
                    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(spacing, spacing));

                    RenderDevicePanel(profile->devices[i], static_cast<int>(i), col_width - spacing * 2);

                    ImGui::PopStyleVar();
                }

                ImGui::EndTable();
            }
        }
    }

    ImGui::End();

    // Rendering
    ImGui::Render();

    int display_w, display_h;
    glfwGetFramebufferSize(window_, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    glClearColor(0.15f, 0.15f, 0.15f, 1.00f);
    glClear(GL_COLOR_BUFFER_BIT);

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    glfwSwapBuffers(window_);
}

void GuiWindow::RenderDevicePanel(const DeviceDef& device, int device_index, float panel_width) {
    ImGui::PushID(device_index);

    // Device panel background
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 5.0f);

    // Device panel as a bordered child window with fixed height to prevent scrolling
    std::string panel_label = "##DevicePanel" + std::to_string(device_index);
    const float panel_height = 400.0f;  // Fixed height to ensure panels fit
    ImGui::BeginChild(panel_label.c_str(), ImVec2(panel_width, panel_height), true);

    ImGui::PopStyleColor();
    ImGui::PopStyleVar();

    // Device header
    std::string device_label = std::string(device.role);
    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "%s", device_label.c_str());
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "(%s)", device.user_path);
    ImGui::Separator();

    // === Active State ===
    bool is_active = false;
    OxPose pose = {{0, 0, 0}, {0, 0, 0, 1}};
    simulator_->GetDevicePose(device.user_path, &pose, &is_active);

    if (!device.always_active) {
        bool active_toggle = is_active;
        if (ImGui::Checkbox("Active", &active_toggle)) {
            simulator_->SetDevicePose(device.user_path, pose, active_toggle);
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Enable/disable device tracking");
        }
    } else {
        ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "Active: Always On");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("This device is always active");
        }
    }

    ImGui::Spacing();
    ImGui::Separator();

    // === Pose Controls (Always Visible) ===
    ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.5f, 1.0f), "Pose");
    ImGui::Spacing();

    // Position
    ImGui::Text("Position:");
    float pos[3] = {pose.position.x, pose.position.y, pose.position.z};
    ImGui::SetNextItemWidth(-1);
    if (ImGui::DragFloat3("##Position", pos, 0.01f, -10.0f, 10.0f, "%.2f")) {
        pose.position = {pos[0], pos[1], pos[2]};
        simulator_->SetDevicePose(device.user_path, pose, is_active);
    }

    ImGui::Spacing();

    // Orientation (as Euler angles for easier editing)
    ImGui::Text("Rotation (Euler XYZ):");

    // Convert quaternion to Euler angles (XYZ order)
    float x = pose.orientation.x;
    float y = pose.orientation.y;
    float z = pose.orientation.z;
    float w = pose.orientation.w;

    // Roll (X-axis rotation)
    float sinr_cosp = 2.0f * (w * x + y * z);
    float cosr_cosp = 1.0f - 2.0f * (x * x + y * y);
    float roll = std::atan2(sinr_cosp, cosr_cosp);

    // Pitch (Y-axis rotation)
    float sinp = 2.0f * (w * y - z * x);
    float pitch = std::abs(sinp) >= 1.0f ? std::copysign(3.14159265f / 2.0f, sinp) : std::asin(sinp);

    // Yaw (Z-axis rotation)
    float siny_cosp = 2.0f * (w * z + x * y);
    float cosy_cosp = 1.0f - 2.0f * (y * y + z * z);
    float yaw = std::atan2(siny_cosp, cosy_cosp);

    float euler[3] = {roll * 57.2958f, pitch * 57.2958f, yaw * 57.2958f};  // Convert to degrees
    ImGui::SetNextItemWidth(-1);
    if (ImGui::DragFloat3("##Orientation", euler, 1.0f, -180.0f, 180.0f, "%.1f°")) {
        // Convert Euler back to quaternion
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

    // Reset button
    if (ImGui::Button("Reset Pose", ImVec2(-1, 0))) {
        simulator_->SetDevicePose(device.user_path, device.default_pose, is_active);
    }

    // === Input Components (Always Visible) ===
    if (!device.components.empty()) {
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.5f, 1.0f), "Input Components");
        ImGui::Spacing();

        for (const auto& component : device.components) {
            RenderComponentControl(device, component, device_index);
        }
    }

    ImGui::EndChild();
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
