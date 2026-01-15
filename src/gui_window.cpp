#include "gui_window.h"

#include <iostream>

namespace ox_sim {

GuiWindow::GuiWindow() : simulator_(nullptr), running_(false) {}

GuiWindow::~GuiWindow() { Stop(); }

bool GuiWindow::Start(SimulatorCore* simulator) {
    if (!simulator) {
        return false;
    }

    simulator_ = simulator;

    // TODO: Implement GUI using ImGui + SDL or similar
    std::cout << "GUI Mode: Not yet implemented (TBD)" << std::endl;
    std::cout << "Please use API mode with 'mode': 'api' in config.json" << std::endl;

    running_ = true;
    return true;
}

void GuiWindow::Stop() { running_ = false; }

}  // namespace ox_sim
