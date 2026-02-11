// Objective-C++ implementation for macOS native window styling
// This file must be compiled with Objective-C++ compiler (.mm extension)

#ifdef __APPLE__

#import <Cocoa/Cocoa.h>
#define GLFW_EXPOSE_NATIVE_COCOA
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <iostream>

extern "C" void ApplyMacOSNativeStyle(GLFWwindow* window) {
    @autoreleasepool {
        NSWindow* nsWindow = glfwGetCocoaWindow(window);
        if (!nsWindow) {
            std::cerr << "Warning: Failed to get NSWindow from GLFW window" << std::endl;
            return;
        }

        // NOTE: We cannot use NSVisualEffectView with GLFW's OpenGL rendering because:
        // 1. GLFW creates an NSOpenGLView as the content view
        // 2. Adding NSVisualEffectView as a subview interferes with OpenGL rendering
        // 3. The transparent framebuffer + vibrancy causes content to disappear
        //
        // Instead, we apply native macOS appearance through window properties only.

        // ALWAYS use dark mode background (consistent with dark ImGui theme)
        // Never use light mode as it would clash with dark UI colors
        [nsWindow setBackgroundColor:[NSColor colorWithCalibratedWhite:0.18 alpha:1.0]];
        std::cout << "Applied macOS dark mode window background (content visible)" << std::endl;

        // Enable native-looking title bar (non-transparent, standard appearance)
        nsWindow.titlebarAppearsTransparent = NO;

        // Ensure the window is opaque so OpenGL content renders properly
        // CRITICAL: This prevents transparency issues that would hide content
        [nsWindow setOpaque:YES];

        // Make window accept mouse moved events for better interaction
        [nsWindow setAcceptsMouseMovedEvents:YES];

        std::cout << "macOS window styling complete - OpenGL content visibility guaranteed" << std::endl;
    }
}

#endif  // __APPLE__
