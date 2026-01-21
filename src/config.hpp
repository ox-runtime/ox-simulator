#pragma once

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "crow/json.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

// Configuration structure
struct SimulatorConfig {
    std::string device = "oculus_quest_2";
    std::string mode = "api";
    int api_port = 8765;
};

// Global simulator state (defined in driver.cpp)
extern SimulatorConfig g_config;

// Helper function to read config file
inline bool LoadConfig(const std::string& config_path) {
    std::ifstream file(config_path);
    if (!file.is_open()) {
        std::cout << "Config file not found at: " << config_path << std::endl;
        std::cout << "Using defaults: device=oculus_quest_2, mode=api, port=8765" << std::endl;
        return false;
    }

    // Read entire file into string
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string config_str = buffer.str();
    file.close();

    // Parse JSON using CROW
    auto json = crow::json::load(config_str);
    if (!json) {
        std::cerr << "ERROR: Invalid JSON in config file: " << config_path << std::endl;
        std::cerr << "Using default configuration" << std::endl;
        return false;
    }

    // Extract configuration with validation
    if (json.has("device") && json["device"].t() == crow::json::type::String) {
        g_config.device = json["device"].s();
    }

    if (json.has("mode") && json["mode"].t() == crow::json::type::String) {
        g_config.mode = json["mode"].s();
        // Validate mode
        if (g_config.mode != "api" && g_config.mode != "gui") {
            std::cerr << "WARNING: Invalid mode '" << g_config.mode << "', defaulting to 'api'" << std::endl;
            g_config.mode = "api";
        }
    }

    if (json.has("api_port") && json["api_port"].t() == crow::json::type::Number) {
        g_config.api_port = static_cast<int>(json["api_port"].d());
        // Validate port range
        if (g_config.api_port < 1024 || g_config.api_port > 65535) {
            std::cerr << "WARNING: Invalid port " << g_config.api_port << ", using default 8765" << std::endl;
            g_config.api_port = 8765;
        }
    }

    std::cout << "Loaded config: device=" << g_config.device << ", mode=" << g_config.mode
              << ", port=" << g_config.api_port << std::endl;

    return true;
}

// Helper to get module directory path
inline std::filesystem::path get_module_path() {
#ifdef _WIN32
    HMODULE module = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCWSTR>(&get_module_path), &module);

    wchar_t path[MAX_PATH];
    GetModuleFileNameW(module, path, MAX_PATH);
    return std::filesystem::path(path).parent_path();
#else
    Dl_info info{};
    dladdr(reinterpret_cast<void*>(&get_module_path), &info);
    return std::filesystem::path(info.dli_fname).parent_path();
#endif
}

// Get the config file path (same directory as driver)
inline std::string GetConfigPath() { return (get_module_path() / "config.json").string(); }