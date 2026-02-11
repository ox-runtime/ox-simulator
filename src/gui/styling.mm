// Minimal macOS helper for titlebar appearance
#ifdef __APPLE__

#import <Cocoa/Cocoa.h>
#define GLFW_EXPOSE_NATIVE_COCOA
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

extern "C" void SetTitleBarAppearance(GLFWwindow* window, bool dark) {
    NSWindow* nsWindow = glfwGetCocoaWindow(window);
    if (nsWindow) {
        if (@available(macOS 10.14, *)) {
            nsWindow.appearance = [NSAppearance appearanceNamed:dark ? NSAppearanceNameDarkAqua : NSAppearanceNameAqua];
        }
    }
}

extern "C" bool IsSystemDarkModeObjC() {
    if (@available(macOS 10.14, *)) {
        NSString* osxMode = [[NSUserDefaults standardUserDefaults] stringForKey:@"AppleInterfaceStyle"];
        return [osxMode isEqualToString:@"Dark"];
    }
    return false;
}

#endif
