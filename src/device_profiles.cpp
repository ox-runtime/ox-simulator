#include "device_profiles.h"

#include <cmath>
#include <map>
#include <stdexcept>

namespace ox_sim {

// Define device profiles
static const DeviceProfile QUEST_2_PROFILE = {DeviceType::OCULUS_QUEST_2,

                                              // Device info
                                              "Meta Quest 2 (Simulated)", "Meta Platforms", "QUEST2-SIM",
                                              0x2833,  // Meta VID
                                              0x0186,  // Quest 2 PID

                                              // Display: 1832x1920 per eye
                                              1832, 1920, 1832, 1920,
                                              90.0f,  // 90Hz (also supports 120Hz)

                                              // FOV (approximate Quest 2 values in radians)
                                              -0.785398f,  // left: ~45 degrees
                                              0.785398f,   // right: ~45 degrees
                                              0.872665f,   // up: ~50 degrees
                                              -0.872665f,  // down: ~50 degrees

                                              // Tracking
                                              true,  // position tracking
                                              true,  // orientation tracking

                                              // Controllers
                                              true, "/interaction_profiles/oculus/touch_controller"};

static const DeviceProfile QUEST_3_PROFILE = {DeviceType::OCULUS_QUEST_3,

                                              // Device info
                                              "Meta Quest 3 (Simulated)", "Meta Platforms", "QUEST3-SIM", 0x2833,
                                              0x0200,

                                              // Display: 2064x2208 per eye
                                              2064, 2208, 2064, 2208,
                                              120.0f,  // 120Hz

                                              // FOV (Quest 3 has slightly wider FOV)
                                              -0.872665f,  // left: ~50 degrees
                                              0.872665f,   // right: ~50 degrees
                                              0.959931f,   // up: ~55 degrees
                                              -0.959931f,  // down: ~55 degrees

                                              true, true, true, "/interaction_profiles/oculus/touch_controller"};

static const DeviceProfile VIVE_PROFILE = {DeviceType::HTC_VIVE,

                                           "HTC Vive (Simulated)", "HTC Corporation", "VIVE-SIM",
                                           0x0BB4,  // HTC VID
                                           0x2C87,

                                           // Display: 1080x1200 per eye
                                           1080, 1200, 1080, 1200, 90.0f,

                                           // FOV
                                           -0.785398f, 0.785398f, 0.872665f, -0.872665f,

                                           true, true, true, "/interaction_profiles/htc/vive_controller"};

static const DeviceProfile INDEX_PROFILE = {DeviceType::VALVE_INDEX,

                                            "Valve Index HMD (Simulated)", "Valve Corporation", "INDEX-SIM",
                                            0x28DE,  // Valve VID
                                            0x2012,

                                            // Display: 1440x1600 per eye
                                            1440, 1600, 1440, 1600,
                                            144.0f,  // 144Hz

                                            // FOV (Index has wide FOV)
                                            -0.959931f,  // ~55 degrees
                                            0.959931f, 0.959931f, -0.959931f,

                                            true, true, true, "/interaction_profiles/valve/index_controller"};

// Map for easy lookup
static const std::map<std::string, DeviceType> NAME_TO_TYPE = {
    {"oculus_quest_2", DeviceType::OCULUS_QUEST_2},
    {"oculus_quest_3", DeviceType::OCULUS_QUEST_3},
    {"htc_vive", DeviceType::HTC_VIVE},
    {"valve_index", DeviceType::VALVE_INDEX},
};

const DeviceProfile& GetDeviceProfile(DeviceType type) {
    switch (type) {
        case DeviceType::OCULUS_QUEST_2:
            return QUEST_2_PROFILE;
        case DeviceType::OCULUS_QUEST_3:
            return QUEST_3_PROFILE;
        case DeviceType::HTC_VIVE:
            return VIVE_PROFILE;
        case DeviceType::VALVE_INDEX:
            return INDEX_PROFILE;
        default:
            throw std::runtime_error("Unknown device type");
    }
}

const DeviceProfile* GetDeviceProfileByName(const std::string& name) {
    auto it = NAME_TO_TYPE.find(name);
    if (it != NAME_TO_TYPE.end()) {
        return &GetDeviceProfile(it->second);
    }
    return nullptr;
}

}  // namespace ox_sim
