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

        // Set window background color to match macOS native dark theme
        // This shows through the transparent framebuffer, giving a native look
        // CONTENT VISIBILITY: Window background doesn't interfere with OpenGL rendering
        if (@available(macOS 10.14, *)) {
            // macOS 10.14+: Use system colors that adapt to light/dark mode
            [nsWindow setBackgroundColor:[NSColor windowBackgroundColor]];
            std::cout << "Applied macOS 10.14+ native window background (content visible)" << std::endl;
        } else {
            // macOS < 10.14: Use a static dark color for consistency
            [nsWindow setBackgroundColor:[NSColor colorWithCalibratedWhite:0.18 alpha:1.0]];
            std::cout << "Applied macOS < 10.14 native window background (content visible)" << std::endl;
        }
        
        // Enable native-looking title bar (non-transparent, standard appearance)
        nsWindow.titlebarAppearsTransparent = NO;

        // Ensure the window is opaque so OpenGL content renders properly
        // CRITICAL: This prevents transparency issues that would hide content
        [nsWindow setOpaque:YES];

        // Make window accept mouse moved events for better interaction
        [nsWindow setAcceptsMouseMovedEvents:YES];
    }
}

#endif  // __APPLE__
