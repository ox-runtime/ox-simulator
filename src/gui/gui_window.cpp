#include "gui_window.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <thread>

#include "window_contents.hpp"

// GLFW and OpenGL
#include <GLFW/glfw3.h>

// Dear ImGui
#include "device_profiles.h"
#include "frame_data.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "styling.hpp"
#include "utils.hpp"

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

    ImGuiIO& io = ImGui::GetIO();

    // Main loop
    while (!should_stop_ && !glfwWindowShouldClose(window_)) {
        glfwPollEvents();

        // Small sleep to avoid burning CPU
        std::this_thread::sleep_for(std::chrono::milliseconds(16));  // ~60 FPS

        RenderFrame();

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

}  // namespace ox_sim
