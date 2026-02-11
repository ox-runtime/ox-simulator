#include "gui_window.h"

#include <chrono>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <thread>

// GLFW and OpenGL
#include <GLFW/glfw3.h>

// Platform-specific includes for dark titlebar
#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include <dwmapi.h>
#include <windows.h>
#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif
#pragma comment(lib, "dwmapi.lib")
#endif

#ifdef __APPLE__
#define GLFW_EXPOSE_NATIVE_COCOA
#include <GLFW/glfw3native.h>
extern "C" void SetTitleBarAppearance(GLFWwindow* window, bool dark);
extern "C" bool IsSystemDarkModeObjC();
#endif

#ifdef __linux__
#define GLFW_EXPOSE_NATIVE_X11
#include <GLFW/glfw3native.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#endif

// Dear ImGui
#include "device_profiles.h"
#include "frame_data.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

namespace ox_sim {

// Default font size for the GUI
#ifdef _WIN32
const float DEFAULT_FONT_SIZE = 17.0f;
#endif
#ifdef __APPLE__
const float DEFAULT_FONT_SIZE = 13.0f;
#endif
#ifdef __linux__
const float DEFAULT_FONT_SIZE = 16.0f;
#endif

// Get the appropriate Arial-like font path for the current platform
std::string GetFontPath() {
#ifdef _WIN32
    return "C:/Windows/Fonts/arial.ttf";
#endif
#ifdef __APPLE__
    return "/System/Library/Fonts/Helvetica.ttc";
#endif
#ifdef __linux__
    // Try multiple Arial-like fonts in order of preference
    const char* linux_font_paths[] = {
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",  // Liberation Sans (most common)
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",                  // DejaVu Sans
        "/usr/share/fonts/truetype/ubuntu/Ubuntu-R.ttf",                    // Ubuntu
        "/usr/share/fonts/truetype/freefont/FreeSans.ttf"                   // FreeSans
    };
    for (const char* path : linux_font_paths) {
        // Check if file exists (simple check using filesystem if available)
        if (std::filesystem::exists(path)) {
            return path;
        }
    }
    return "";  // No font found
#endif
    return "";  // Fallback
}

// UI color definitions for theming
struct UIColors {
    ImVec4 background_color;
    ImVec4 header_color;
    ImVec4 device_info_color;
    ImVec4 status_color;
    ImVec4 device_label_color;
    ImVec4 user_path_color;
    ImVec4 always_active_color;
    ImVec4 section_header_color;
    ImVec4 api_port_color;

    // Theme semantic colors
    ImVec4 base;
    ImVec4 mantle;
    ImVec4 surface0;
    ImVec4 surface1;
    ImVec4 surface2;
    ImVec4 overlay0;
    ImVec4 overlay2;
    ImVec4 text;
    ImVec4 subtext0;
    ImVec4 accent;
    ImVec4 accent_soft;
    ImVec4 accent_hover;
    ImVec4 accent_active;
    ImVec4 warn;
    ImVec4 ok;
    ImVec4 bg;
    ImVec4 surface;
    ImVec4 border;
    ImVec4 text_dim;

    // Additional theme colors
    ImVec4 window_bg;
    ImVec4 child_bg;
    ImVec4 border_shadow;
    ImVec4 frame_bg_active;
    ImVec4 scrollbar_grab_active;
    ImVec4 button_active;
    ImVec4 header_active;
    ImVec4 separator_hovered;
    ImVec4 separator_active;
    ImVec4 resize_grip_active;
    ImVec4 table_row_bg;
    ImVec4 table_row_bg_alt;
    ImVec4 text_selected_bg;
    ImVec4 nav_windowing_highlight;
    ImVec4 nav_windowing_dim_bg;
    ImVec4 modal_window_dim_bg;
};

UIColors theme_colors;

// Detect if system is in dark mode (cross-platform)
bool is_system_dark_mode() {
#ifdef _WIN32
    // Windows 10+: Check registry for system theme
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", 0,
                      KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD value = 0;
        DWORD size = sizeof(value);
        if (RegQueryValueExW(hKey, L"AppsUseLightTheme", nullptr, nullptr, (LPBYTE)&value, &size) == ERROR_SUCCESS) {
            RegCloseKey(hKey);
            return value == 0;  // 0 = dark mode
        }
        RegCloseKey(hKey);
    }
    return false;  // Default to light on Windows 7 or if registry fails
#elif defined(__APPLE__)
    // macOS: Check NSUserDefaults for dark mode (works on 10.14+)
    return IsSystemDarkModeObjC();
#elif defined(__linux__)
    // Linux: Check GTK theme preference
    const char* gtk_theme = std::getenv("GTK_THEME");
    if (gtk_theme && std::string(gtk_theme).find("dark") != std::string::npos) {
        return true;
    }
    // Also check gsettings if available (common on modern Linux)
    FILE* pipe = popen("gsettings get org.gnome.desktop.interface gtk-theme 2>/dev/null", "r");
    if (pipe) {
        char buffer[256];
        if (fgets(buffer, sizeof(buffer), pipe)) {
            std::string theme(buffer);
            pclose(pipe);
            return theme.find("dark") != std::string::npos || theme.find("Dark") != std::string::npos;
        }
        pclose(pipe);
    }
    return false;  // Default to light
#endif
    return false;
}

void setup_dark_theme_colors(UIColors& colors) {
    colors.base = ImVec4(0.18f, 0.18f, 0.18f, 1.0f);         // #2E2E2E
    colors.mantle = ImVec4(0.15f, 0.15f, 0.15f, 1.0f);       // #262626
    colors.surface0 = ImVec4(0.05f, 0.05f, 0.05f, 1.0f);     // #0D0D0D
    colors.surface1 = ImVec4(0.25f, 0.25f, 0.25f, 1.0f);     // #404040
    colors.surface2 = ImVec4(0.29f, 0.29f, 0.29f, 1.0f);     // #4A4A4A
    colors.overlay0 = ImVec4(0.40f, 0.40f, 0.40f, 1.0f);     // #666666
    colors.overlay2 = ImVec4(0.55f, 0.55f, 0.55f, 1.0f);     // #8C8C8C
    colors.text = ImVec4(0.86f, 0.86f, 0.86f, 1.0f);         // #DBDBDB
    colors.subtext0 = ImVec4(0.72f, 0.72f, 0.72f, 1.0f);     // #B8B8B8
    colors.accent = ImVec4(0.26f, 0.62f, 0.95f, 1.0f);       // #429EF2
    colors.accent_soft = ImVec4(0.20f, 0.48f, 0.78f, 1.0f);  // #337AC7
    colors.warn = ImVec4(0.96f, 0.78f, 0.36f, 1.0f);         // #F5C75C
    colors.ok = ImVec4(0.40f, 0.74f, 0.40f, 1.0f);           // #66BD66

    // Dark theme equivalents for light theme fields
    colors.bg = ImVec4(0.08f, 0.08f, 0.08f, 1.0f);             // Dark background
    colors.surface = colors.surface0;                          // Use surface0 for surface in dark theme
    colors.border = colors.surface1;                           // Use surface1 for border in dark theme
    colors.text_dim = ImVec4(0.55f, 0.55f, 0.55f, 1.0f);       // Dim text
    colors.accent_hover = ImVec4(0.30f, 0.68f, 0.98f, 1.0f);   // Lighter accent
    colors.accent_active = ImVec4(0.18f, 0.52f, 0.88f, 1.0f);  // Active accent

    // ImGui color assignments for dark theme
    colors.window_bg = colors.base;  // Use base for window bg in dark
    colors.child_bg = ImVec4(1.0f, 1.0f, 1.0f, 0.04f);
    colors.frame_bg_active = colors.surface2;
    colors.scrollbar_grab_active = colors.overlay2;
    colors.button_active = colors.overlay0;
    colors.header_active = colors.surface2;
    colors.separator_hovered = colors.overlay0;
    colors.separator_active = colors.overlay2;
    colors.resize_grip_active = colors.overlay2;
    colors.table_row_bg = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    colors.table_row_bg_alt = ImVec4(1.0f, 1.0f, 1.0f, 0.06f);
    colors.text_selected_bg = colors.overlay0;
    colors.nav_windowing_highlight = ImVec4(1.0f, 1.0f, 1.0f, 0.7f);
    colors.nav_windowing_dim_bg = ImVec4(0.8f, 0.8f, 0.8f, 0.2f);
    colors.modal_window_dim_bg = ImVec4(0.0f, 0.0f, 0.0f, 0.35f);

    // Set UI colors for dark theme
    colors.background_color = colors.bg;
    colors.header_color = colors.accent;
    colors.device_info_color = ImVec4(0.8f, 0.9f, 0.8f, 1.0f);  // Light green for device info
    colors.status_color = colors.ok;
    colors.device_label_color = colors.accent_soft;
    colors.user_path_color = colors.subtext0;
    colors.always_active_color = colors.ok;
    colors.section_header_color = colors.warn;
    colors.api_port_color = colors.subtext0;
}

void setup_light_theme_colors(UIColors& colors) {
    colors.bg = ImVec4(0.95f, 0.95f, 0.96f, 1.0f);             // #F2F2F5
    colors.surface = ImVec4(0.98f, 0.98f, 0.99f, 1.0f);        // #FAFAFC
    colors.surface1 = ImVec4(0.92f, 0.92f, 0.94f, 1.0f);       // #EBEBF0
    colors.surface2 = ImVec4(0.88f, 0.88f, 0.90f, 1.0f);       // #E0E0E6
    colors.border = ImVec4(0.75f, 0.75f, 0.78f, 1.0f);         // #BFBFC7
    colors.text = ImVec4(0.15f, 0.15f, 0.18f, 1.0f);           // #26262E
    colors.text_dim = ImVec4(0.45f, 0.45f, 0.48f, 1.0f);       // #73737A
    colors.accent = ImVec4(0.20f, 0.50f, 0.88f, 1.0f);         // #3380E0
    colors.accent_hover = ImVec4(0.28f, 0.58f, 0.92f, 1.0f);   // #4794EB
    colors.accent_active = ImVec4(0.15f, 0.42f, 0.80f, 1.0f);  // #266BCC
    colors.warn = ImVec4(0.90f, 0.65f, 0.20f, 1.0f);           // #E6A633
    colors.ok = ImVec4(0.28f, 0.68f, 0.35f, 1.0f);             // #47AD59

    // ImGui color assignments for light theme
    colors.window_bg = colors.bg;
    colors.child_bg = colors.surface;
    colors.frame_bg_active = colors.surface2;
    colors.scrollbar_grab_active = ImVec4(0.70f, 0.70f, 0.73f, 1.0f);
    colors.button_active = ImVec4(0.82f, 0.82f, 0.85f, 1.0f);
    colors.header_active = ImVec4(0.85f, 0.85f, 0.88f, 1.0f);
    colors.separator_hovered = ImVec4(0.65f, 0.65f, 0.70f, 1.0f);
    colors.separator_active = ImVec4(0.55f, 0.55f, 0.60f, 1.0f);
    colors.resize_grip_active = ImVec4(0.70f, 0.70f, 0.73f, 1.0f);
    colors.table_row_bg = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    colors.table_row_bg_alt = ImVec4(0.0f, 0.0f, 0.0f, 0.03f);
    colors.text_selected_bg = ImVec4(0.20f, 0.50f, 0.88f, 0.35f);
    colors.nav_windowing_highlight = ImVec4(0.15f, 0.15f, 0.18f, 0.7f);
    colors.nav_windowing_dim_bg = ImVec4(0.2f, 0.2f, 0.2f, 0.2f);
    colors.modal_window_dim_bg = ImVec4(0.0f, 0.0f, 0.0f, 0.35f);

    // Set UI colors for light theme
    colors.background_color = colors.bg;
    colors.header_color = colors.accent;
    colors.device_info_color = ImVec4(0.2f, 0.4f, 0.2f, 1.0f);  // Light green for device info
    colors.status_color = colors.ok;
    colors.device_label_color = colors.accent_hover;
    colors.user_path_color = colors.text_dim;
    colors.always_active_color = colors.ok;
    colors.section_header_color = colors.warn;
    colors.api_port_color = colors.text_dim;
}

void setup_theme_colors(UIColors& colors) {
    if (is_system_dark_mode()) {
        setup_dark_theme_colors(colors);
    } else {
        setup_light_theme_colors(colors);
    }
}

void setup_theme_styling() {
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    bool is_light = theme_colors.bg.x > 0.5f;

    // Assign theme colors to ImGui
    colors[ImGuiCol_WindowBg] = theme_colors.window_bg;
    colors[ImGuiCol_ChildBg] = theme_colors.child_bg;
    colors[ImGuiCol_PopupBg] = theme_colors.surface;
    colors[ImGuiCol_Border] = theme_colors.border;
    colors[ImGuiCol_BorderShadow] = theme_colors.border_shadow;
    colors[ImGuiCol_FrameBg] = theme_colors.surface1;
    colors[ImGuiCol_FrameBgHovered] = theme_colors.surface2;
    colors[ImGuiCol_FrameBgActive] = theme_colors.frame_bg_active;
    colors[ImGuiCol_TitleBg] = theme_colors.mantle;
    colors[ImGuiCol_TitleBgActive] = theme_colors.surface0;
    colors[ImGuiCol_TitleBgCollapsed] = theme_colors.mantle;
    colors[ImGuiCol_MenuBarBg] = theme_colors.mantle;
    colors[ImGuiCol_ScrollbarBg] = theme_colors.surface;
    colors[ImGuiCol_ScrollbarGrab] = theme_colors.surface2;
    colors[ImGuiCol_ScrollbarGrabHovered] = is_light ? theme_colors.border : theme_colors.overlay0;
    colors[ImGuiCol_ScrollbarGrabActive] = theme_colors.scrollbar_grab_active;
    colors[ImGuiCol_CheckMark] = theme_colors.ok;
    colors[ImGuiCol_SliderGrab] = theme_colors.accent_soft;
    colors[ImGuiCol_SliderGrabActive] = theme_colors.accent;
    colors[ImGuiCol_Button] = theme_colors.surface1;
    colors[ImGuiCol_ButtonHovered] = theme_colors.surface2;
    colors[ImGuiCol_ButtonActive] = is_light ? theme_colors.button_active : theme_colors.overlay0;
    colors[ImGuiCol_Header] = theme_colors.surface1;
    colors[ImGuiCol_HeaderHovered] = theme_colors.surface2;
    colors[ImGuiCol_HeaderActive] = theme_colors.header_active;
    colors[ImGuiCol_Separator] = is_light ? theme_colors.border : theme_colors.surface1;
    colors[ImGuiCol_SeparatorHovered] = is_light ? theme_colors.separator_hovered : theme_colors.overlay0;
    colors[ImGuiCol_SeparatorActive] = is_light ? theme_colors.separator_active : theme_colors.overlay2;
    colors[ImGuiCol_ResizeGrip] = theme_colors.surface2;
    colors[ImGuiCol_ResizeGripHovered] = is_light ? theme_colors.border : theme_colors.overlay0;
    colors[ImGuiCol_ResizeGripActive] = is_light ? theme_colors.resize_grip_active : theme_colors.overlay2;
    colors[ImGuiCol_Tab] = is_light ? theme_colors.surface1 : theme_colors.surface0;
    colors[ImGuiCol_TabHovered] = theme_colors.surface2;
    colors[ImGuiCol_TabActive] = is_light ? theme_colors.bg : theme_colors.surface1;
    colors[ImGuiCol_TabUnfocused] = is_light ? theme_colors.surface1 : theme_colors.surface0;
    colors[ImGuiCol_TabUnfocusedActive] = is_light ? theme_colors.surface2 : theme_colors.surface1;
    colors[ImGuiCol_PlotLines] = theme_colors.accent;
    colors[ImGuiCol_PlotLinesHovered] = theme_colors.warn;
    colors[ImGuiCol_PlotHistogram] = theme_colors.accent_soft;
    colors[ImGuiCol_PlotHistogramHovered] = theme_colors.ok;
    colors[ImGuiCol_TableHeaderBg] = theme_colors.surface1;
    colors[ImGuiCol_TableBorderStrong] = theme_colors.border;
    colors[ImGuiCol_TableBorderLight] = is_light ? theme_colors.surface2 : theme_colors.surface0;
    colors[ImGuiCol_TableRowBg] = theme_colors.table_row_bg;
    colors[ImGuiCol_TableRowBgAlt] = theme_colors.table_row_bg_alt;
    colors[ImGuiCol_TextSelectedBg] = theme_colors.text_selected_bg;
    colors[ImGuiCol_DragDropTarget] = theme_colors.warn;
    colors[ImGuiCol_NavHighlight] = theme_colors.accent;
    colors[ImGuiCol_NavWindowingHighlight] = theme_colors.nav_windowing_highlight;
    colors[ImGuiCol_NavWindowingDimBg] = theme_colors.nav_windowing_dim_bg;
    colors[ImGuiCol_ModalWindowDimBg] = theme_colors.modal_window_dim_bg;
    colors[ImGuiCol_Text] = theme_colors.text;
    colors[ImGuiCol_TextDisabled] = theme_colors.subtext0;

    // Rounded corners
    style.WindowRounding = 4.0f;
    style.ChildRounding = 5.0f;
    style.FrameRounding = 3.0f;
    style.PopupRounding = 3.0f;
    style.ScrollbarRounding = 6.0f;
    style.GrabRounding = 3.0f;
    style.TabRounding = 3.0f;

    // Padding and spacing
    style.WindowPadding = ImVec2(10.0f, 10.0f);
    style.FramePadding = ImVec2(6.0f, 4.0f);
    style.ItemSpacing = ImVec2(10.0f, 6.0f);
    style.ItemInnerSpacing = ImVec2(6.0f, 4.0f);
    style.IndentSpacing = 18.0f;
    style.ScrollbarSize = 14.0f;
    style.GrabMinSize = 10.0f;

    // Borders
    style.WindowBorderSize = 0.0f;  // Remove window border
    style.ChildBorderSize = 1.0f;
    style.PopupBorderSize = 1.0f;
    style.FrameBorderSize = 1.0f;
    style.TabBorderSize = 0.0f;

    style.AntiAliasedLines = true;
    style.AntiAliasedFill = true;
}

void setup_theme() {
    setup_theme_colors(theme_colors);
    setup_theme_styling();
}

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

    // Create window with graphics context
    window_ = glfwCreateWindow(1280, 720, "ox simulator", nullptr, nullptr);
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

    // Load platform-specific Arial-like font
    std::string font_path = GetFontPath();
    ImFont* default_font = nullptr;

#ifdef __APPLE__
    // Retina display support: Detect framebuffer scale and adjust font rendering
    int window_width, window_height;
    int framebuffer_width, framebuffer_height;
    glfwGetWindowSize(window_, &window_width, &window_height);
    glfwGetFramebufferSize(window_, &framebuffer_width, &framebuffer_height);

    // Calculate scale factor (typically 2.0 on Retina, 1.0 on non-Retina)
    float scale_x = (float)framebuffer_width / (float)window_width;
    float scale_y = (float)framebuffer_height / (float)window_height;
    float font_scale = (scale_x + scale_y) * 0.5f;  // Average of x and y scale

    if (font_scale > 1.1f) {
        // Retina display detected - load font at higher resolution for crisp rendering
        if (!font_path.empty()) {
            // Load font at scaled size for Retina displays
            default_font = io.Fonts->AddFontFromFileTTF(font_path.c_str(), DEFAULT_FONT_SIZE * font_scale);
            // Scale back down in ImGui to get crisp rendering at native resolution
            io.FontGlobalScale = 1.0f / font_scale;
            std::cout << "Retina display detected (scale: " << font_scale << "x) - using high-DPI font rendering"
                      << std::endl;
        }
    } else {
        // Non-Retina display - standard font loading
        if (!font_path.empty()) {
            default_font = io.Fonts->AddFontFromFileTTF(font_path.c_str(), DEFAULT_FONT_SIZE);
        }
    }
#else
    // Windows/Linux - standard font loading
    if (!font_path.empty()) {
        default_font = io.Fonts->AddFontFromFileTTF(font_path.c_str(), DEFAULT_FONT_SIZE);
    }
#endif

    if (default_font) {
        io.FontDefault = default_font;
    } else {
        io.FontDefault = io.Fonts->AddFontDefault();
    }

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

    // --- Frame Preview ---
    RenderFramePreview();

    // ========== TOP TOOLBAR (Fixed Height) ==========
    const float toolbar_height = 120.0f;
    ImGui::BeginChild("Toolbar", ImVec2(0, toolbar_height), true, ImGuiWindowFlags_NoScrollbar);

    // Header
    ImGui::PushFont(ImGui::GetFont());
    ImGui::TextColored(theme_colors.header_color, "Device");
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
    ImGui::TextColored(theme_colors.api_port_color, "(Port: 8765)");

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
        ImGui::TextColored(theme_colors.device_info_color, "Device: %s", profile->name);
        ImGui::SameLine(300);
        ImGui::Text("Manufacturer: %s", profile->manufacturer);
        ImGui::SameLine(600);
        ImGui::Text("Display: %dx%d @ %.0f Hz", profile->display_width, profile->display_height, profile->refresh_rate);
    }

    ImGui::TextColored(theme_colors.status_color, "Status: %s", status_message_.c_str());

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
                    RenderDevicePanel(profile->devices[i], static_cast<int>(i), col_width - spacing * 2);
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

    // Clear with solid background
    glClearColor(theme_colors.background_color.x, theme_colors.background_color.y, theme_colors.background_color.z,
                 theme_colors.background_color.w);
    glClear(GL_COLOR_BUFFER_BIT);

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    glfwSwapBuffers(window_);
}

void GuiWindow::RenderFramePreview() {
    // Update preview textures from driver frame data
    UpdateFrameTextures();

    // Draw preview if available
    if (preview_textures_valid_ && preview_width_ > 0 && preview_height_ > 0) {
        ImGui::Text("Frame Preview:");
        float preview_w = 320.0f;
        float preview_h = preview_w * (float)preview_height_ / (float)preview_width_;
        for (int eye = 0; eye < 2; ++eye) {
            if (preview_textures_[eye]) {
                ImGui::Image((ImTextureID)(intptr_t)preview_textures_[eye], ImVec2(preview_w, preview_h));
                ImGui::SameLine();
            }
        }
        ImGui::NewLine();
    }
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

    // Device panel as a bordered child window with fixed height to prevent scrolling
    std::string panel_label = "##DevicePanel" + std::to_string(device_index);
    const float panel_height = 400.0f;  // Fixed height to ensure panels fit
    ImGui::BeginChild(panel_label.c_str(), ImVec2(panel_width, panel_height), true);

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
        ImGui::TextColored(theme_colors.section_header_color, "Input Components");
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
