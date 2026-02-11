#pragma once

// Forward declare GLFWwindow to avoid including GLFW headers
struct GLFWwindow;

namespace ox_sim {

// Platform-specific window styling functions
namespace PlatformStyling {

// Detect platform and version, then apply appropriate native styling
void ApplyNativeWindowStyle(GLFWwindow* window);

// Platform-specific detection functions
bool IsWindows7OrLater();
bool IsWindows10OrLater();
bool IsWindows11OrLater();
bool IsMacOS10_14OrLater();

}  // namespace PlatformStyling

}  // namespace ox_sim
