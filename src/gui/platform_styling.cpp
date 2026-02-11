#include "platform_styling.h"

#include <iostream>

// GLFW must be included before platform-specific native headers
#include <GLFW/glfw3.h>

// Platform-specific includes
#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include <dwmapi.h>
#include <windows.h>
#pragma comment(lib, "dwmapi.lib")

// Windows DWM constants (for compatibility with older SDKs)
#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

#ifndef DWMWA_SYSTEMBACKDROP_TYPE
#define DWMWA_SYSTEMBACKDROP_TYPE 38
#endif

// Backdrop type constants (not always defined in older Windows SDKs)
// The enum DWM_SYSTEMBACKDROP_TYPE may or may not exist depending on SDK version
#ifndef DWMSBT_AUTO
#define DWMSBT_AUTO 0
#define DWMSBT_NONE 1
#define DWMSBT_MAINWINDOW 2  // Mica
#define DWMSBT_TRANSIENTWINDOW 3
#define DWMSBT_TABBEDWINDOW 4
#endif

#endif

#ifdef __linux__
#define GLFW_EXPOSE_NATIVE_X11
#include <GLFW/glfw3native.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>

#include <cstring>
#endif

#ifdef __APPLE__
#include <AvailabilityMacros.h>
// Forward declare the Objective-C++ function
extern "C" void ApplyMacOSNativeStyle(GLFWwindow* window);
#endif

namespace ox_sim {
namespace PlatformStyling {

#ifdef _WIN32

// Use RtlGetVersion for accurate Windows version detection (not fooled by compatibility shims)
typedef LONG(WINAPI* RtlGetVersionPtr)(PRTL_OSVERSIONINFOW);

static bool GetRealWindowsVersion(DWORD& majorVersion, DWORD& minorVersion, DWORD& buildNumber) {
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    if (!ntdll) {
        return false;
    }

    RtlGetVersionPtr RtlGetVersion = (RtlGetVersionPtr)GetProcAddress(ntdll, "RtlGetVersion");
    if (!RtlGetVersion) {
        return false;
    }

    RTL_OSVERSIONINFOW osvi = {};
    osvi.dwOSVersionInfoSize = sizeof(osvi);

    if (RtlGetVersion(&osvi) == 0) {  // STATUS_SUCCESS
        majorVersion = osvi.dwMajorVersion;
        minorVersion = osvi.dwMinorVersion;
        buildNumber = osvi.dwBuildNumber;
        return true;
    }

    return false;
}

bool IsWindows7OrLater() {
    DWORD major = 0, minor = 0, build = 0;
    if (!GetRealWindowsVersion(major, minor, build)) {
        return false;
    }
    // Windows 7 is version 6.1
    return (major > 6) || (major == 6 && minor >= 1);
}

bool IsWindows10OrLater() {
    DWORD major = 0, minor = 0, build = 0;
    if (!GetRealWindowsVersion(major, minor, build)) {
        return false;
    }
    // Windows 10 is version 10.0
    return major >= 10;
}

bool IsWindows11OrLater() {
    DWORD major = 0, minor = 0, build = 0;
    if (!GetRealWindowsVersion(major, minor, build)) {
        return false;
    }
    // Windows 11 is version 10.0 build 22000+
    std::cout << "Windows version: " << major << "." << minor << " build " << build << std::endl;
    return (major == 10 && build >= 22000) || major > 10;
}

static void ApplyWindowsNativeStyle(GLFWwindow* window) {
    HWND hwnd = glfwGetWin32Window(window);
    if (!hwnd) {
        std::cerr << "Warning: Failed to get native Windows handle" << std::endl;
        return;
    }

    // Enable dark mode title bar (Windows 10 1809+)
    if (IsWindows10OrLater()) {
        BOOL darkMode = TRUE;
        DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &darkMode, sizeof(darkMode));
        std::cout << "Applied Windows dark mode title bar" << std::endl;
    }

    // Apply backdrop effect based on Windows version
    // CONTENT VISIBILITY: All Windows paths preserve content rendering
    if (IsWindows11OrLater()) {
        // Windows 11: Use Mica backdrop (renders behind content, doesn't hide it)
        int backdropType = DWMSBT_MAINWINDOW;  // Mica
        HRESULT hr = DwmSetWindowAttribute(hwnd, DWMWA_SYSTEMBACKDROP_TYPE, &backdropType, sizeof(backdropType));
        if (SUCCEEDED(hr)) {
            std::cout << "Applied Windows 11 Mica backdrop (content visible)" << std::endl;
        } else {
            std::cerr << "Warning: Failed to apply Mica backdrop (HRESULT: 0x" << std::hex << hr << ")" << std::endl;
            std::cerr << "Note: Mica requires Windows 11 22H2+ and may not work in all window configurations"
                      << std::endl;
            std::cout << "Continuing with standard window (content visible)" << std::endl;
        }
    } else if (IsWindows10OrLater()) {
        // Windows 10: Just use dark mode title bar, don't apply blur
        // (Blur-behind was deprecated and doesn't work well on Windows 10)
        // SAFE: Only affects title bar, content fully visible
        std::cout << "Applied Windows 10 dark mode (content visible)" << std::endl;
    } else if (IsWindows7OrLater()) {
        // Windows 7-8: Use DWM blur-behind (but don't extend frame to avoid transparency issues)
        // SAFE: We explicitly avoid extending the frame, so content remains visible
        DWM_BLURBEHIND bb = {};
        bb.dwFlags = DWM_BB_ENABLE;
        bb.fEnable = TRUE;
        bb.hRgnBlur = nullptr;

        HRESULT hr = DwmEnableBlurBehindWindow(hwnd, &bb);
        if (SUCCEEDED(hr)) {
            std::cout << "Applied Windows 7/8 Aero blur-behind effect (content visible)" << std::endl;
        } else {
            std::cerr << "Warning: Failed to apply Aero effect (HRESULT: 0x" << std::hex << hr << ")" << std::endl;
        }
        // Note: Not extending frame to avoid making window completely transparent
    } else {
        // Windows < 7 (unlikely but possible): No styling, standard window
        std::cout << "Windows < 7: Using standard window (content visible)" << std::endl;
    }
}

#elif defined(__APPLE__)

bool IsMacOS10_14OrLater() {
    // macOS 10.14 (Mojave) introduced dark mode
    // Check using MAC_OS_X_VERSION_MIN_REQUIRED or runtime check
#if MAC_OS_X_VERSION_MIN_REQUIRED >= 101400
    return true;
#else
    // Runtime check would be more accurate but requires Objective-C
    // For now, assume we're on a modern macOS if compiling for it
    return true;
#endif
}

#elif defined(__linux__)

static void ApplyLinuxNativeStyle(GLFWwindow* window) {
    // Linux: Set window properties for native desktop integration
    // CONTENT VISIBILITY: Only sets window manager hints, doesn't affect rendering
    Display* display = glfwGetX11Display();
    Window x11_window = glfwGetX11Window(window);

    if (!display || !x11_window) {
        std::cerr << "Warning: Failed to get X11 display/window" << std::endl;
        std::cerr << "Continuing without X11 hints (content still visible)" << std::endl;
        return;
    }

    // Set _NET_WM_WINDOW_TYPE to _NET_WM_WINDOW_TYPE_NORMAL for proper window manager treatment
    Atom window_type = XInternAtom(display, "_NET_WM_WINDOW_TYPE", False);
    Atom window_type_normal = XInternAtom(display, "_NET_WM_WINDOW_TYPE_NORMAL", False);
    XChangeProperty(display, x11_window, window_type, XA_ATOM, 32, PropModeReplace, (unsigned char*)&window_type_normal,
                    1);

    // Request compositing if available (for transparency and effects)
    // This is handled by the window manager, but we can hint that we support it
    Atom net_wm_state = XInternAtom(display, "_NET_WM_STATE", False);

    // Flush changes
    XFlush(display);

    std::cout << "Applied Linux window manager hints for native integration (content visible)" << std::endl;
}

#endif

void ApplyNativeWindowStyle(GLFWwindow* window) {
    if (!window) {
        std::cerr << "Warning: Cannot apply native style - window is null" << std::endl;
        return;
    }

#ifdef _WIN32
    std::cout << "Applying Windows native styling..." << std::endl;
    ApplyWindowsNativeStyle(window);
#elif defined(__APPLE__)
    std::cout << "Applying macOS native styling..." << std::endl;
    // CONTENT VISIBILITY: All macOS paths ensure content remains visible
    // Call Objective-C++ implementation (handles both >= 10.14 and < 10.14)
    ApplyMacOSNativeStyle(window);
#elif defined(__linux__)
    std::cout << "Applying Linux native styling..." << std::endl;
    ApplyLinuxNativeStyle(window);
#else
    // Unknown platform fallback
    // CONTENT VISIBILITY: No styling applied, standard GLFW window with full content visibility
    std::cout << "Unknown platform - using standard window styling (content visible)" << std::endl;
#endif
}

// Stub implementations for non-Windows platforms
#ifndef _WIN32
bool IsWindows7OrLater() { return false; }
bool IsWindows10OrLater() { return false; }
bool IsWindows11OrLater() { return false; }
#endif

#ifndef __APPLE__
bool IsMacOS10_14OrLater() { return false; }
#endif

}  // namespace PlatformStyling
}  // namespace ox_sim
