#pragma once

#include <atomic>
#include <memory>
#include <thread>

#include "simulator_core.h"

// Forward declare CROW app
namespace crow {
template <typename... Middlewares>
class Crow;
using SimpleApp = Crow<>;
}  // namespace crow

namespace ox_sim {

class HttpServer {
   public:
    HttpServer();
    ~HttpServer();

    // Start server in background thread
    // device_profile_ptr: pointer to the current device profile pointer (can be updated for device switching)
    bool Start(SimulatorCore* simulator, const DeviceProfile** device_profile_ptr, int port = 8765);

    // Stop server
    void Stop();

    bool IsRunning() const { return running_.load(); }

   private:
    void ServerThread();

    SimulatorCore* simulator_;
    const DeviceProfile** device_profile_ptr_;  // Pointer to device profile pointer (for switching)
    int port_;
    std::thread server_thread_;
    std::atomic<bool> running_;
    std::atomic<bool> should_stop_;
};

}  // namespace ox_sim
