#pragma once

#include <atomic>
#include <string>
#include <thread>

#include "simulator_core.h"

// Forward declarations to avoid including GLFW/ImGui headers in header
struct GLFWwindow;

namespace ox_sim {

class GuiWindow {
   public:
    GuiWindow();
    ~GuiWindow();

    // Start GUI window (creates window and starts rendering thread)
    // device_profile_ptr: pointer to the current device profile pointer (can be updated for device switching)
    // api_enabled: pointer to shared API enable state
    bool Start(SimulatorCore* simulator, const DeviceProfile** device_profile_ptr, bool* api_enabled);

    // Stop GUI (signals thread to exit and waits for it)
    void Stop();

    bool IsRunning() const { return running_; }

   private:
    // GUI rendering thread function
    void RenderLoop();

    // Initialize OpenGL and ImGui
    bool InitializeGraphics();

    // Cleanup OpenGL and ImGui
    void CleanupGraphics();

    // Render one frame
    void RenderFrame();

    // Render device-specific panels
    void RenderDevicePanel(const DeviceDef& device, int device_index, float panel_width);

    // Render component controls
    void RenderComponentControl(const DeviceDef& device, const ComponentDef& component, int device_index);

    SimulatorCore* simulator_;
    const DeviceProfile** device_profile_ptr_;  // Pointer to device profile pointer (for switching)
    bool* api_enabled_;                         // Pointer to API enable state (shared with driver)
    std::atomic<bool> running_;
    std::atomic<bool> should_stop_;
    std::thread render_thread_;
    GLFWwindow* window_;

    // UI state
    int selected_device_type_;  // For dropdown
    std::string status_message_;
};

}  // namespace ox_sim
