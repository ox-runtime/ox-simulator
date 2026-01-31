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
    std::cout << "Opening a window..." << std::endl;

    running_ = true;
    return true;
}

void GuiWindow::Stop() { running_ = false; }

}  // namespace ox_sim
