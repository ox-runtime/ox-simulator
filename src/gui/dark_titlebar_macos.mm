// Minimal macOS helper to set dark titlebar
#ifdef __APPLE__

#import <Cocoa/Cocoa.h>
#define GLFW_EXPOSE_NATIVE_COCOA
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

extern "C" void SetDarkTitleBar(GLFWwindow* window) {
    NSWindow* nsWindow = glfwGetCocoaWindow(window);
    if (nsWindow) {
        if (@available(macOS 10.14, *)) {
            nsWindow.appearance = [NSAppearance appearanceNamed:NSAppearanceNameDarkAqua];
        }
    }
}

#endif
