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

        // macOS 10.14+ (Mojave): Use NSVisualEffectView for native backdrop
        if (@available(macOS 10.14, *)) {
            // Create a visual effect view that fills the entire content view
            NSVisualEffectView* effectView = [[NSVisualEffectView alloc] initWithFrame:[[nsWindow contentView] bounds]];
            
            // Set material based on macOS version for best compatibility
            if (@available(macOS 10.14, *)) {
                // macOS 10.14+: Use appropriate material
                // NSVisualEffectMaterialUnderWindowBackground gives a nice native look
                // that works well across light and dark modes
                effectView.material = NSVisualEffectMaterialUnderWindowBackground;
                effectView.blendingMode = NSVisualEffectBlendingModeBehindWindow;
                effectView.state = NSVisualEffectStateFollowsWindowActiveState;
                
                std::cout << "Applied macOS 10.14+ visual effect material (UnderWindowBackground)" << std::endl;
            }
            
            // Make it resize with the window
            effectView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
            
            // Insert the effect view at the bottom of the view hierarchy
            // This allows ImGui content to render on top
            [[nsWindow contentView] addSubview:effectView positioned:NSWindowBelow relativeTo:nil];
            
            // Enable title bar transparency and full-size content view
            nsWindow.titlebarAppearsTransparent = NO;  // Keep title bar visible but native
            nsWindow.styleMask |= NSWindowStyleMaskFullSizeContentView;
            
            std::cout << "Applied macOS native window styling with vibrancy effect" << std::endl;
        } else {
            // macOS < 10.14: Fallback to standard window appearance
            std::cout << "macOS < 10.14: Using standard window appearance (no vibrancy)" << std::endl;
        }
        
        // Set window background color to work well with transparency
        [nsWindow setBackgroundColor:[NSColor colorWithCalibratedWhite:0.18 alpha:1.0]];
        
        // Make window accept mouse moved events for better interaction
        [nsWindow setAcceptsMouseMovedEvents:YES];
    }
}

#endif  // __APPLE__
