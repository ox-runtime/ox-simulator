#pragma once

#include <filesystem>
#include <string>

// Configuration structure
struct SimulatorConfig {
    std::string device = "oculus_quest_2";
    std::string mode = "api";
    int api_port = 8765;
};

// Global simulator state
extern SimulatorConfig g_config;

// Forward declarations for helper functions
bool LoadConfig(const std::string& config_path);
std::filesystem::path get_module_path();
std::string GetConfigPath();