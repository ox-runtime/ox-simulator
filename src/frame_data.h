#pragma once

#include <ox_driver.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <deque>
#include <mutex>

using namespace std::chrono;

namespace ox_sim {

// Frame data for preview (zero-copy - uses shared memory pointers from runtime)
struct FrameData {
    const void* pixel_data[2] = {nullptr, nullptr};  // Left and right eye (shared memory pointers)
    uint32_t data_size[2] = {0, 0};                  // Size of pixel data in bytes
    uint32_t width = 0;
    uint32_t height = 0;
    std::atomic<bool> has_new_frame{false};
    std::mutex mutex;

    // --- Session state ---
    std::atomic<uint32_t> session_state{OX_SESSION_STATE_UNKNOWN};

    bool IsSessionActive() const {
        OxSessionState s = static_cast<OxSessionState>(session_state.load(std::memory_order_relaxed));
        return s == OX_SESSION_STATE_SYNCHRONIZED || s == OX_SESSION_STATE_VISIBLE || s == OX_SESSION_STATE_FOCUSED;
    }

    // --- App frame-rate ---
    std::atomic<uint32_t> app_fps{0};
    int64_t last_frame_time_ms = 0;
    std::deque<int64_t> dt_history;  // sliding window of last 10 frame durations (ms)

   public:
    void UpdateFps() {
        // This is called once per xrEndFrame
        int64_t now_ms = duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();

        if (last_frame_time_ms > 0) {
            int64_t dt_ms = now_ms - last_frame_time_ms;
            if (dt_ms > 0) {
                dt_history.push_back(dt_ms);
                if (dt_history.size() > 10) dt_history.pop_front();

                int64_t avg_dt_ms = 0;
                for (int64_t dt : dt_history) avg_dt_ms += dt;
                avg_dt_ms /= static_cast<int64_t>(dt_history.size());

                uint32_t fps = static_cast<uint32_t>(1000.0 / static_cast<double>(avg_dt_ms));
                app_fps.store(fps, std::memory_order_relaxed);
            }
        }

        last_frame_time_ms = now_ms;
    }
};

// Get the global frame data - implemented in driver.cpp
FrameData* GetFrameData();

}  // namespace ox_sim
