cmake_minimum_required(VERSION 3.20)
project(cs2_skinchanger LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Output as DLL
add_library(${PROJECT_NAME} SHARED)

# ============================================================================
# SOURCE FILES
# ============================================================================

target_sources(${PROJECT_NAME} PRIVATE
    # Entry point
    src/dllmain.cpp

    # Core utilities
    src/core/memory.cpp

    # SDK
    src/sdk/schema.cpp
    src/sdk/inventory.cpp

    # Features
    src/features/skinchanger/items.cpp
    src/features/skinchanger/config.cpp
    src/features/skinchanger/skinchanger.cpp

    # UI
    src/ui/imgui_backend.cpp
    src/ui/skin_panel.cpp
)

# ============================================================================
# INCLUDE DIRECTORIES
# ============================================================================

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/imgui
    ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/imgui/backends
)

# ============================================================================
# IMGUI SOURCES (compile directly)
# ============================================================================

target_sources(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/imgui/imgui.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/imgui/imgui_draw.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/imgui/imgui_tables.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/imgui/imgui_widgets.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/imgui/backends/imgui_impl_dx11.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/imgui/backends/imgui_impl_win32.cpp
)

# ============================================================================
# COMPILE OPTIONS
# ============================================================================

target_compile_options(${PROJECT_NAME} PRIVATE
    /W4
    /WX-
    /permissive-
    /Zc:__cplusplus
    /utf-8
)

# Disable specific warnings
target_compile_options(${PROJECT_NAME} PRIVATE
    /wd4100  # unreferenced formal parameter
    /wd4201  # nonstandard extension used: nameless struct/union
    /wd4324  # structure was padded due to alignment specifier
)

# ============================================================================
# LINK LIBRARIES
# ============================================================================

target_link_libraries(${PROJECT_NAME} PRIVATE
    d3d11.lib
    dxgi.lib
    psapi.lib
)

# ============================================================================
# OUTPUT CONFIGURATION
# ============================================================================

set_target_properties(${PROJECT_NAME} PROPERTIES
    OUTPUT_NAME "skinchanger"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/build"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/build"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/build"
)

# ============================================================================
# DEPENDENCY SETUP
# ============================================================================

# Check for ImGui
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/dependencies/imgui/imgui.cpp")
    message(WARNING "ImGui not found. Clone ImGui into dependencies/imgui/")
    message(WARNING "  git clone https://github.com/ocornut/imgui.git dependencies/imgui")
endif()

# ============================================================================
# POST-BUILD
# ============================================================================

# Optional: Copy DLL to CS2 directory after build
# set(CS2_DIR "C:/Program Files (x86)/Steam/steamapps/common/Counter-Strike Global Offensive/game/bin/win64/")
# add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
#     COMMAND ${CMAKE_COMMAND} -E copy
#         $<TARGET_FILE:${PROJECT_NAME}>
#         "${CS2_DIR}/skinchanger.dll"
# )
