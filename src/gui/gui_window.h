#pragma once

#include <cstdint>
#include <string>

#include "simulator_core.h"
#include "vog.h"

namespace ox_sim {

class GuiWindow {
   public:
    GuiWindow();
    ~GuiWindow();

    // Start the GUI window. device_profile_ptr points at the current device profile
    // pointer so that device switching is reflected immediately; api_enabled is a
    // shared flag for the HTTP API server toggle.
    bool Start(SimulatorCore* simulator, const DeviceProfile** device_profile_ptr, bool* api_enabled);

    // Signal the window to close and wait for it to finish.
    void Stop();

    bool IsRunning() const { return window_.IsRunning(); }

   private:
    // ImGui widget callback — called once per frame by vog::Window.
    void RenderFrame();

    void RenderDevicePanel(const DeviceDef& device, int device_index, float panel_width);
    void RenderComponentControl(const DeviceDef& device, const ComponentDef& component, int device_index);
    void RenderFramePreview();
    void UpdateFrameTextures();

    vog::Window window_;

    SimulatorCore* simulator_ = nullptr;
    const DeviceProfile** device_profile_ptr_ = nullptr;
    bool* api_enabled_ = nullptr;

    // UI state
    int selected_device_type_ = 0;
    int preview_eye_selection_ = 0;
    std::string status_message_{"Ready"};

    // Frame preview textures (OpenGL texture IDs)
    uint32_t preview_textures_[2] = {0, 0};
    uint32_t preview_width_ = 0;
    uint32_t preview_height_ = 0;
    bool preview_textures_valid_ = false;
};

}  // namespace ox_sim
