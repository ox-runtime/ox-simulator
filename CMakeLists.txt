cmake_minimum_required(VERSION 3.15)
project(ox-driver-simulator VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(OUTPUT_FOLDER "simulator_driver")

# Source files
set(DRIVER_SOURCES
    src/driver.cpp
    src/simulator_core.cpp
    src/device_profiles.cpp
    src/http_server.cpp
    src/gui_window.cpp
)

# Driver shared library - output as driver.dll / libdriver.so
add_library(simulator_driver SHARED ${DRIVER_SOURCES})

# Include directories
target_include_directories(simulator_driver PRIVATE 
    src
    ${CMAKE_SOURCE_DIR}  # For ox_driver.h in root directory
    ${CMAKE_SOURCE_DIR}/third_party  # For third-party headers (crow, asio)
)

# Platform-specific settings
if(WIN32)
    # Windows requires ws2_32 for networking and _WIN32_WINNT definition for ASIO
    target_link_libraries(simulator_driver PRIVATE ws2_32)
    target_compile_definitions(simulator_driver PRIVATE _WIN32_WINNT=0x0601)  # Windows 7+
endif()

# Rename to standardized driver library name
set_target_properties(simulator_driver PROPERTIES
    OUTPUT_NAME "driver"
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${OUTPUT_FOLDER}
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/${OUTPUT_FOLDER}
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/${OUTPUT_FOLDER}
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${OUTPUT_FOLDER}
    LIBRARY_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/${OUTPUT_FOLDER}
    LIBRARY_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/${OUTPUT_FOLDER}
)

# Copy config.json to output directory
add_custom_command(TARGET simulator_driver POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/config.json
        ${CMAKE_BINARY_DIR}/${OUTPUT_FOLDER}/config.json
    COMMENT "Copying config.json to output directory"
)

# Print build completion message
add_custom_command(TARGET simulator_driver POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "====================================================================="
    COMMAND ${CMAKE_COMMAND} -E echo "ox Simulator Driver build complete!"
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "Output: ${CMAKE_BINARY_DIR}/${OUTPUT_FOLDER}"
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "Copy ${CMAKE_BINARY_DIR}/${OUTPUT_FOLDER} to your ox runtime's 'drivers' folder"
    COMMAND ${CMAKE_COMMAND} -E echo "Edit config.json to select device type and interface mode"
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "====================================================================="
    VERBATIM
)
