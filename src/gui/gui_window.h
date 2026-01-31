#pragma once

#include <atomic>
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
    bool Start(SimulatorCore* simulator);

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

    SimulatorCore* simulator_;
    std::atomic<bool> running_;
    std::atomic<bool> should_stop_;
    std::thread render_thread_;
    GLFWwindow* window_;
};

}  // namespace ox_sim
