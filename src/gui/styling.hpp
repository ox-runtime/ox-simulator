#pragma once

#include <GLFW/glfw3.h>

#include <cmath>
#include <filesystem>
#include <iostream>
#include <string>

#include "imgui.h"

#ifdef _WIN32
#include <windows.h>
#endif

#ifdef __linux__
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#endif

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

namespace ox_sim {

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

// Global theme colors instance
UIColors theme_colors;

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
inline std::string GetFontPath() {
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

// Load and configure fonts for ImGui
inline void setup_fonts(ImGuiIO& io, GLFWwindow* window) {
    // Load platform-specific Arial-like font
    std::string font_path = GetFontPath();
    ImFont* default_font = nullptr;

#ifdef __APPLE__
    // Retina display support: Detect framebuffer scale and adjust font rendering
    int window_width, window_height;
    int framebuffer_width, framebuffer_height;
    glfwGetWindowSize(window, &window_width, &window_height);
    glfwGetFramebufferSize(window, &framebuffer_width, &framebuffer_height);

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
}

// Detect if system is in dark mode (cross-platform)
inline bool is_system_dark_mode() {
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

inline void setup_dark_theme_colors(UIColors& colors) {
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

inline void setup_light_theme_colors(UIColors& colors) {
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

inline void setup_theme_colors(UIColors& colors) {
    if (is_system_dark_mode()) {
        setup_dark_theme_colors(colors);
    } else {
        setup_light_theme_colors(colors);
    }
}

inline void setup_theme_styling() {
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

inline void setup_theme() {
    setup_theme_colors(theme_colors);
    setup_theme_styling();
}

}  // namespace ox_sim