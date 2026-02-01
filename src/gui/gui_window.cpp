#include "gui_window.h"

#include <chrono>
#include <iostream>
#include <thread>

// GLFW and OpenGL
#include <GLFW/glfw3.h>

// Dear ImGui
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

namespace ox_sim {

// GLFW error callback
static void glfw_error_callback(int error, const char* description) {
    std::cerr << "GLFW Error " << error << ": " << description << std::endl;
}

GuiWindow::GuiWindow() : simulator_(nullptr), running_(false), should_stop_(false), window_(nullptr) {}

GuiWindow::~GuiWindow() { Stop(); }

bool GuiWindow::Start(SimulatorCore* simulator) {
    if (!simulator) {
        std::cerr << "GuiWindow::Start: Simulator is null" << std::endl;
        return false;
    }

    if (running_) {
        std::cerr << "GuiWindow::Start: GUI already running" << std::endl;
        return false;
    }

    simulator_ = simulator;
    should_stop_ = false;

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

    // === Dear ImGui UI ===

    // Main window - centered and sized appropriately
    ImGui::SetNextWindowPos(ImVec2(50, 50), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_FirstUseEver);

    ImGui::Begin("ox Simulator Control Panel", nullptr);

    // Device info
    if (simulator_ && simulator_->GetProfile()) {
        const DeviceProfile* profile = simulator_->GetProfile();
        ImGui::TextColored(ImVec4(0.3f, 0.7f, 1.0f, 1.0f), "ox Simulator Control Panel");
        ImGui::Separator();
        ImGui::Text("Device: %s", profile->name);
        ImGui::Text("Manufacturer: %s", profile->manufacturer);
        ImGui::Separator();
    }

    // Status
    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Status: Running");
    ImGui::Spacing();
    ImGui::Text("This GUI demonstrates Dear ImGui working correctly on:");
    ImGui::BulletText("Windows (x64)");
    ImGui::BulletText("macOS (Intel & Apple Silicon, 10.13+)");
    ImGui::BulletText("Linux (x64)");
    ImGui::Separator();

    // Platform info - determine at compile time
    const char* platform_name = "Unknown";
#if defined(_WIN32)
    platform_name = "Windows";
#elif defined(__APPLE__)
#if defined(__aarch64__) || defined(__arm64__)
    platform_name = "macOS (Apple Silicon)";
#else
    platform_name = "macOS (Intel)";
#endif
#elif defined(__linux__)
    platform_name = "Linux";
#endif

    ImGui::Text("Current Platform: %s", platform_name);

    const GLubyte* gl_version = glGetString(GL_VERSION);
    const GLubyte* gl_renderer = glGetString(GL_RENDERER);
    ImGui::Text("Graphics API: OpenGL %s", gl_version ? (const char*)gl_version : "Unknown");
    ImGui::Text("Renderer: %s", gl_renderer ? (const char*)gl_renderer : "Unknown");
    ImGui::Separator();

    ImGui::Spacing();
    ImGui::TextWrapped("Use the HTTP API to control the simulator or close this window to run in headless mode.");
    ImGui::Spacing();

    // Close button
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

}  // namespace ox_sim
