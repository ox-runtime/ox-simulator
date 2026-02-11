# ===== Dependencies =====

# Fetch GLFW
include(FetchContent)

# GLFW for cross-platform windowing
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(glfw)

# Dear ImGui (manually add files)
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.91.7
)
FetchContent_MakeAvailable(imgui)

# Set ImGui sources (core + backends we need)
set(IMGUI_SOURCES
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)

# ===== GUI Sources =====

set(GUI_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/gui_window.cpp
    ${IMGUI_SOURCES}
)

# Add minimal macOS helper for dark titlebar
if(APPLE)
    list(APPEND GUI_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/styling.mm
    )
    set_source_files_properties(
        ${CMAKE_CURRENT_SOURCE_DIR}/styling.mm
        PROPERTIES
        COMPILE_FLAGS "-x objective-c++"
    )
endif()

set(GUI_SOURCES ${GUI_SOURCES} PARENT_SCOPE)

set(GUI_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
    ${glfw_SOURCE_DIR}/include
    PARENT_SCOPE
)

# Export dependencies for main target
set(GUI_LIBRARIES
    glfw
    OpenGL::GL
)

# Platform-specific GUI libraries (required by GLFW)
if(APPLE)
    find_library(COCOA_LIBRARY Cocoa REQUIRED)
    find_library(IOKIT_LIBRARY IOKit REQUIRED)
    find_library(COREVIDEO_LIBRARY CoreVideo REQUIRED)
    list(APPEND GUI_LIBRARIES
        ${COCOA_LIBRARY}
        ${IOKIT_LIBRARY}
        ${COREVIDEO_LIBRARY}
    )
elseif(WIN32)
    # Windows: dwmapi for dark titlebar
    list(APPEND GUI_LIBRARIES dwmapi)
elseif(UNIX)
    # Linux: X11 required by GLFW
    find_package(X11)
    if(X11_FOUND)
        list(APPEND GUI_LIBRARIES ${X11_LIBRARIES})
    endif()
endif()

set(GUI_LIBRARIES ${GUI_LIBRARIES} PARENT_SCOPE)