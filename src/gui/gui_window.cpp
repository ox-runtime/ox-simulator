#include "gui_window.h"

#include <chrono>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <thread>

// GLFW and OpenGL
#include <GLFW/glfw3.h>

// Dear ImGui
#include "device_profiles.h"
#include "frame_data.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "styling.hpp"

namespace ox_sim {

const int WINDOW_WIDTH = 1280;
const int WINDOW_HEIGHT = 720;

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
      preview_eye_selection_(0),
      status_message_("Ready"),
      preview_width_(0),
      preview_height_(0),
      preview_textures_valid_(false) {
    preview_textures_[0] = 0;
    preview_textures_[1] = 0;
}

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

    glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);

    // Create window with graphics context
    window_ = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "ox simulator", nullptr, nullptr);
    if (window_ == nullptr) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return false;
    }

    // Detect system dark mode and apply matching titlebar
    bool dark_mode = is_system_dark_mode();

#ifdef _WIN32
    // Windows: Set titlebar to match system theme (works on Windows 10 1809+)
    HWND hwnd = glfwGetWin32Window(window_);
    if (hwnd) {
        BOOL useDark = dark_mode ? TRUE : FALSE;
        DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &useDark, sizeof(useDark));
    }
#elif defined(__APPLE__)
    // macOS: Set window appearance to match system (works on macOS 10.14+)
    SetTitleBarAppearance(window_, dark_mode);
#elif defined(__linux__)
    // Linux: Set GTK theme variant (hints to window manager)
    Display* display = glfwGetX11Display();
    Window x11_window = glfwGetX11Window(window_);
    if (display && x11_window) {
        Atom gtk_theme_variant = XInternAtom(display, "_GTK_THEME_VARIANT", False);
        Atom utf8_string = XInternAtom(display, "UTF8_STRING", False);
        const char* variant = dark_mode ? "dark" : "light";
        XChangeProperty(display, x11_window, gtk_theme_variant, utf8_string, 8, PropModeReplace,
                        (const unsigned char*)variant, strlen(variant));
        XFlush(display);
    }
#endif

    glfwMakeContextCurrent(window_);
    glfwSwapInterval(1);  // Enable vsync

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls

    // Apply appropriate theme based on detected dark mode
    setup_theme();

    // Load and configure fonts
    setup_fonts(io, window_);

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
    ImGui::PushStyleColor(ImGuiCol_ChildBg, theme_colors.surface);
    ImGui::BeginChild("TopToolbar", ImVec2(0, top_toolbar_h), false, ImGuiWindowFlags_NoScrollbar);
    {
        // Estimates for centering
        const float btn_runtime_w = 190.0f;
        const float btn_api_w = 160.0f;
        const float lbl_device_w = ImGui::CalcTextSize("Device:").x + style.ItemSpacing.x;
        const float combo_device_w = 190.0f;
        const float spacing = style.ItemSpacing.x * 2.0f;
        const float total_w = btn_runtime_w + spacing + btn_api_w + spacing + lbl_device_w + combo_device_w;
        const ImVec2 avail = ImGui::GetContentRegionAvail();

        float start_x = (avail.x - total_w) * 0.5f;
        if (start_x < 0.0f) start_x = 0.0f;
        float center_y = (avail.y - ImGui::GetFrameHeight()) * 0.5f;
        if (center_y < 0.0f) center_y = 0.0f;
        ImGui::SetCursorPos(ImVec2(start_x, center_y));

        // "Set as OpenXR Runtime" button
        if (ImGui::Button("Set as OpenXR Runtime", ImVec2(btn_runtime_w, 0))) {
            // Register ox runtime as the active OpenXR runtime
#ifdef _WIN32
            HKEY hKey;
            if (RegCreateKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Khronos\\OpenXR\\1", 0, nullptr, REG_OPTION_NON_VOLATILE,
                                KEY_SET_VALUE, nullptr, &hKey, nullptr) == ERROR_SUCCESS) {
                // Runtime JSON path is typically alongside the executable; update if needed
                const std::string runtime_json = "ox_openxr.json";
                RegSetValueExA(hKey, "ActiveRuntime", 0, REG_SZ, reinterpret_cast<const BYTE*>(runtime_json.c_str()),
                               static_cast<DWORD>(runtime_json.size() + 1));
                RegCloseKey(hKey);
                status_message_ = "Registered as active OpenXR runtime";
            } else {
                status_message_ = "Failed to set runtime (try running as Administrator)";
            }
#else
            status_message_ = "Set as active OpenXR runtime (set XR_RUNTIME_JSON env var)";
#endif
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Register ox simulator as the active OpenXR runtime on this system");
        }

        ImGui::SameLine(0, spacing);

        // "Enable API Server" toggle button
        bool api_on = *api_enabled_;
        if (api_on) {
            ImGui::PushStyleColor(ImGuiCol_Button, theme_colors.accent);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, theme_colors.accent_hover);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, theme_colors.accent_active);
        }
        if (ImGui::Button(api_on ? "API Server: ON" : "API Server: OFF", ImVec2(btn_api_w, 0))) {
            *api_enabled_ = !*api_enabled_;
            status_message_ = *api_enabled_ ? "API Server enabled (port 8765)" : "API Server disabled";
        }
        if (api_on) ImGui::PopStyleColor(3);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Toggle HTTP API server on port 8765");
        }

        ImGui::SameLine(0, spacing);

        // Device profile dropdown
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Device:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(combo_device_w);
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
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();

    // ========== MAIN AREA: Preview (left) + Sidebar (right) ==========
    const float sidebar_w = 360.0f;
    const float status_bar_h = 30.0f;
    const float preview_w = content_size.x - sidebar_w - style.ItemSpacing.x;
    const float main_area_h = content_size.y - top_toolbar_h - status_bar_h - style.ItemSpacing.y;

    // Remove any ItemSpacing gap between toolbar and main area
    ImGui::SetCursorPosY(top_toolbar_h);

    // Preview area - add padding by reducing child size and positioning
    const float preview_padding = 5.0f;
    ImGui::SetCursorPos(ImVec2(preview_padding, top_toolbar_h));
    ImGui::BeginChild("PreviewArea", ImVec2(preview_w - preview_padding, main_area_h), false,
                      ImGuiWindowFlags_NoScrollbar);
    {
        RenderFramePreview();
    }
    ImGui::EndChild();

    // Position sidebar to the right of the original preview area
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
            ImGui::TextColored(theme_colors.device_info_color, "Display: %dx%d @ %.0f Hz  |  %s", p->display_width,
                               p->display_height, p->refresh_rate, status_message_.c_str());
        } else {
            ImGui::TextColored(theme_colors.status_color, "%s", status_message_.c_str());
        }
        ImGui::Unindent(5.0f);
    }
    ImGui::EndChild();

    ImGui::End();

    // Rendering
    ImGui::Render();

    int display_w, display_h;
    glfwGetFramebufferSize(window_, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);

    float xscale, yscale;
    glfwGetWindowContentScale(window_, &xscale, &yscale);

    float fontScale = (xscale + yscale) * 0.5f;
    io.FontGlobalScale = 1.0f / fontScale;

    // Clear with solid background
    glClearColor(theme_colors.background_color.x, theme_colors.background_color.y, theme_colors.background_color.z,
                 theme_colors.background_color.w);
    glClear(GL_COLOR_BUFFER_BIT);

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    glfwSwapBuffers(window_);
}

void GuiWindow::RenderFramePreview() {
    UpdateFrameTextures();

    const ImVec2 region = ImGui::GetContentRegionAvail();
    const float toolbar_h = ImGui::GetFrameHeightWithSpacing();
    const float content_h = region.y - toolbar_h;
    const bool has_image = preview_textures_valid_ && preview_width_ > 0 && preview_height_ > 0;

    // ---- Preview toolbar (right-aligned eye dropdown) ----
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
        ImGui::Combo("##EyeSelect", &preview_eye_selection_, eye_names, 3);
    }
    ImGui::EndChild();

    // ---- Preview image area ----
    ImGui::BeginChild("PreviewContent", ImVec2(0, content_h), false, ImGuiWindowFlags_NoScrollbar);
    {
        const ImVec2 avail = ImGui::GetContentRegionAvail();

        if (!has_image) {
            // No image at all — center the message
            const char* msg = "No image received";
            ImVec2 ts = ImGui::CalcTextSize(msg);
            ImGui::SetCursorPos(ImVec2((avail.x - ts.x) * 0.5f, (avail.y - ts.y) * 0.5f));
            ImGui::TextDisabled("%s", msg);
        } else if (preview_eye_selection_ == 2) {
            // Both eyes side by side, each maintaining aspect ratio
            const float aspect = (float)preview_width_ / (float)preview_height_;
            float w_each = avail.x * 0.5f;
            float h_each = w_each / aspect;
            if (h_each > avail.y) {
                h_each = avail.y;
                w_each = h_each * aspect;
            }
            const float y_off = (avail.y - h_each) * 0.5f;
            const float x_off = (avail.x * 0.5f - w_each);
            // Left eye
            ImGui::SetCursorPos(ImVec2(x_off > 0.0f ? x_off : 0.0f, y_off));
            if (preview_textures_[0]) {
                ImGui::Image((ImTextureID)(intptr_t)preview_textures_[0], ImVec2(w_each, h_each));
            } else {
                ImGui::Dummy(ImVec2(w_each, h_each));
            }
            ImGui::SameLine(0, 0);
            // Right eye
            if (preview_textures_[1]) {
                ImGui::Image((ImTextureID)(intptr_t)preview_textures_[1], ImVec2(w_each, h_each));
            } else {
                ImGui::Dummy(ImVec2(w_each, h_each));
            }
        } else {
            // Single eye
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
                ImGui::Image((ImTextureID)(intptr_t)preview_textures_[eye], ImVec2(img_w, img_h));
            } else {
                ImVec2 ts = ImGui::CalcTextSize(no_msg);
                ImGui::SetCursorPos(ImVec2((avail.x - ts.x) * 0.5f, (avail.y - ts.y) * 0.5f));
                ImGui::TextDisabled("%s", no_msg);
            }
        }
    }
    ImGui::EndChild();
}

void GuiWindow::UpdateFrameTextures() {
    FrameData* frame_data = GetFrameData();
    if (!frame_data) {
        return;
    }

    if (!frame_data->has_new_frame.load(std::memory_order_acquire)) {
        return;
    }

    std::lock_guard<std::mutex> lock(frame_data->mutex);
    uint32_t w = frame_data->width;
    uint32_t h = frame_data->height;

    if (w == 0 || h == 0) {
        return;
    }

    for (int eye = 0; eye < 2; ++eye) {
        if (!frame_data->pixel_data[eye]) {
            continue;
        }
        size_t expected_size = w * h * 4;
        if (frame_data->data_size[eye] != expected_size) {
            continue;
        }

        if (!preview_textures_[eye]) {
            glGenTextures(1, &preview_textures_[eye]);
            glBindTexture(GL_TEXTURE_2D, preview_textures_[eye]);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            std::cout << "[GUI] Created OpenGL texture " << preview_textures_[eye] << " for eye " << eye << std::endl;
        }
        glBindTexture(GL_TEXTURE_2D, preview_textures_[eye]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, frame_data->pixel_data[eye]);
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    preview_width_ = w;
    preview_height_ = h;
    preview_textures_valid_ = true;
    frame_data->has_new_frame.store(false, std::memory_order_release);
}

void GuiWindow::RenderDevicePanel(const DeviceDef& device, int device_index, float panel_width) {
    ImGui::PushID(device_index);

    // Draw an inline bordered panel using BeginGroup + draw list — no fixed height, expands to content
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    const float pad = 8.0f;
    const float rounding = 4.0f;
    const ImVec2 panel_tl = ImGui::GetCursorScreenPos();

    // Indent content inside the border
    ImGui::SetCursorScreenPos(ImVec2(panel_tl.x + pad, panel_tl.y + pad));
    ImGui::BeginGroup();
    ImGui::PushItemWidth(panel_width - pad * 2.0f);

    // Device header
    std::string device_label = std::string(device.role);
    ImGui::TextColored(theme_colors.device_label_color, "%s", device_label.c_str());
    ImGui::SameLine();
    ImGui::TextColored(theme_colors.user_path_color, "(%s)", device.user_path);
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
        ImGui::TextColored(theme_colors.always_active_color, "Active: Always On");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("This device is always active");
        }
    }

    ImGui::Spacing();
    ImGui::Separator();

    // === Pose Controls (Always Visible) ===
    ImGui::TextColored(theme_colors.section_header_color, "Pose");
    ImGui::Spacing();

    // Position
    ImGui::Text("Position:");
    float pos[3] = {pose.position.x, pose.position.y, pose.position.z};
    ImGui::SetNextItemWidth(-FLT_MIN);
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
    ImGui::SetNextItemWidth(-FLT_MIN);
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
    if (ImGui::Button("Reset Pose", ImVec2(-FLT_MIN, 0))) {
        simulator_->SetDevicePose(device.user_path, device.default_pose, is_active);
    }

    // === Input Components (Always Visible) ===
    if (!device.components.empty()) {
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextColored(theme_colors.section_header_color, "Input Components");
        ImGui::Spacing();

        for (const auto& component : device.components) {
            RenderComponentControl(device, component, device_index);
        }
    }

    ImGui::PopItemWidth();
    ImGui::EndGroup();

    // Draw border rectangle around the rendered group
    const ImVec2 group_br = ImGui::GetItemRectMax();
    const ImVec2 panel_br = ImVec2(panel_tl.x + panel_width, group_br.y + pad);
    draw_list->AddRect(panel_tl, panel_br, ImGui::ColorConvertFloat4ToU32(theme_colors.border), rounding);

    // Advance cursor past the panel
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
