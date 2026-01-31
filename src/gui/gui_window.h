#pragma once

#include "simulator_core.h"

namespace ox_sim {

class GuiWindow {
   public:
    GuiWindow();
    ~GuiWindow();

    // Start GUI window
    bool Start(SimulatorCore* simulator);

    // Stop GUI
    void Stop();

    bool IsRunning() const { return running_; }

   private:
    SimulatorCore* simulator_;
    bool running_;
};

}  // namespace ox_sim
