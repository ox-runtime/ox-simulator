#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>

namespace ox_sim {

// Frame data for preview (zero-copy - uses shared memory pointers from runtime)
struct FrameData {
    const void* pixel_data[2] = {nullptr, nullptr};  // Left and right eye (shared memory pointers)
    uint32_t data_size[2] = {0, 0};                  // Size of pixel data in bytes
    uint32_t width = 0;
    uint32_t height = 0;
    std::atomic<bool> has_new_frame{false};
    std::mutex mutex;
};

// Get the global frame data - implemented in driver.cpp
FrameData* GetFrameData();

}  // namespace ox_sim
