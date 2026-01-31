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

    // === Main Control Panel ===
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(1200, 700), ImGuiCond_FirstUseEver);

    ImGui::Begin("ox Simulator Control Panel", nullptr, ImGuiWindowFlags_NoCollapse);

    // Header
    ImGui::TextColored(ImVec4(0.3f, 0.7f, 1.0f, 1.0f), "ox Simulator - Device Control");
    ImGui::Separator();

    // === API Toggle ===
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

    ImGui::Spacing();

    // === Device Selection ===
    ImGui::Text("Device Profile:");
    ImGui::SameLine();

    const char* device_names[] = {"Meta Quest 2", "Meta Quest 3", "HTC Vive", "Valve Index", "HTC Vive Tracker"};

    int current_device = selected_device_type_;
    if (ImGui::Combo("##DeviceSelect", &current_device, device_names, IM_ARRAYSIZE(device_names))) {
        // Device changed - switch profile
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

    ImGui::Spacing();
    ImGui::Separator();

    // === Device Information ===
    if (*device_profile_ptr_) {
        const DeviceProfile* profile = *device_profile_ptr_;

        ImGui::TextColored(ImVec4(0.7f, 0.9f, 0.7f, 1.0f), "Current Device: %s", profile->name);
        ImGui::Text("Manufacturer: %s", profile->manufacturer);
        ImGui::Text("Display: %dx%d @ %.0f Hz", profile->display_width, profile->display_height, profile->refresh_rate);
        ImGui::Text("Devices: %zu", profile->devices.size());
    }

    ImGui::Spacing();
    ImGui::Separator();

    // === Status Bar ===
    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Status: %s", status_message_.c_str());
    ImGui::Spacing();
    ImGui::Separator();

    // === Device Panels ===
    if (*device_profile_ptr_) {
        const DeviceProfile* profile = *device_profile_ptr_;

        // Create a child window with scroll for device panels
        ImGui::BeginChild("DevicePanelsScroll", ImVec2(0, -35), true, ImGuiWindowFlags_HorizontalScrollbar);

        for (size_t i = 0; i < profile->devices.size(); i++) {
            RenderDevicePanel(profile->devices[i], static_cast<int>(i));
            ImGui::Spacing();
        }

        ImGui::EndChild();
    }

    ImGui::Separator();

    // === Footer Buttons ===
    if (ImGui::Button("Close Window", ImVec2(120, 0))) {
        glfwSetWindowShouldClose(window_, GLFW_TRUE);
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

void GuiWindow::RenderDevicePanel(const DeviceDef& device, int device_index) {
    // Create collapsible header for each device
    std::string device_label = std::string(device.role) + " (" + device.user_path + ")";

    if (ImGui::CollapsingHeader(device_label.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::PushID(device_index);

        // Indent content
        ImGui::Indent(20.0f);

        // === Active State ===
        bool is_active = false;
        OxPose pose = {{0, 0, 0}, {0, 0, 0, 1}};
        simulator_->GetDevicePose(device.user_path, &pose, &is_active);

        bool active_toggle = is_active;
        if (!device.always_active) {
            if (ImGui::Checkbox("Active", &active_toggle)) {
                simulator_->SetDevicePose(device.user_path, pose, active_toggle);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Enable/disable device tracking");
            }
        } else {
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Active: Always");
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("This device is always active");
            }
        }

        ImGui::Spacing();

        // === Pose Controls ===
        if (ImGui::TreeNode("Pose")) {
            // Position
            ImGui::Text("Position:");
            float pos[3] = {pose.position.x, pose.position.y, pose.position.z};
            if (ImGui::DragFloat3("##Position", pos, 0.01f, -10.0f, 10.0f, "%.2f")) {
                pose.position = {pos[0], pos[1], pos[2]};
                simulator_->SetDevicePose(device.user_path, pose, is_active);
            }

            // Orientation (as Euler angles for easier editing)
            ImGui::Text("Orientation (Euler XYZ):");

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

            // Reset button
            if (ImGui::Button("Reset Pose")) {
                simulator_->SetDevicePose(device.user_path, device.default_pose, is_active);
            }

            ImGui::TreePop();
        }

        ImGui::Spacing();

        // === Input Components ===
        if (!device.components.empty()) {
            if (ImGui::TreeNode("Input Components")) {
                for (const auto& component : device.components) {
                    RenderComponentControl(device, component, device_index);
                }
                ImGui::TreePop();
            }
        }

        ImGui::Unindent(20.0f);
        ImGui::PopID();
    }
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
            ImGui::SameLine(250);
            ImGui::SetNextItemWidth(200);
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
            ImGui::SameLine(250);
            ImGui::SetNextItemWidth(200);
            if (ImGui::SliderFloat2("##vec2", vec2, -1.0f, 1.0f, "%.2f")) {
                value.x = vec2[0];
                value.y = vec2[1];
                simulator_->SetInputStateVec2(device.user_path, component.path, value);
            }
            break;
        }
    }

    ImGui::PopID();
}

}  // namespace ox_sim
