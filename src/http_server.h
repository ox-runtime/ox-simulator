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
    bool Start(SimulatorCore* simulator, int port = 8765);

    // Stop server
    void Stop();

    bool IsRunning() const { return running_.load(); }

   private:
    void ServerThread();

    SimulatorCore* simulator_;
    int port_;
    std::thread server_thread_;
    std::atomic<bool> running_;
    std::atomic<bool> should_stop_;
};

}  // namespace ox_sim
