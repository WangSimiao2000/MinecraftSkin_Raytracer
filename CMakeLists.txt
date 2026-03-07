cmake_minimum_required(VERSION 3.22)
project(MCSkin_RaytraceRenderer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# MSVC: required flags for Qt6 compatibility and UTF-8 source encoding
if(MSVC)
    add_compile_options(/Zc:__cplusplus /permissive- /utf-8)
endif()
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# ── Options ──────────────────────────────────────────────────────────────────
option(BUILD_TESTS "Build unit tests" ON)
option(ENABLE_VULKAN_RT "Enable Vulkan GPU ray tracing backend" OFF)

# ── Vulkan (optional, for GPU ray tracing) ───────────────────────────────────
if(ENABLE_VULKAN_RT)
    find_package(Vulkan QUIET)
    if(NOT Vulkan_FOUND)
        message(FATAL_ERROR
            "ENABLE_VULKAN_RT is ON but Vulkan SDK was not found.\n"
            "Please install the Vulkan SDK (https://vulkan.lunarg.com/sdk/home) "
            "or set VULKAN_SDK / Vulkan_INCLUDE_DIR / Vulkan_LIBRARY appropriately.\n"
            "To build without GPU ray tracing, set -DENABLE_VULKAN_RT=OFF.")
    endif()
    add_compile_definitions(HAVE_VULKAN_RT)
    message(STATUS "Vulkan GPU ray tracing enabled (Vulkan ${Vulkan_VERSION})")
endif()

# ── Qt6 ──────────────────────────────────────────────────────────────────────
find_package(Qt6 REQUIRED COMPONENTS Widgets OpenGL OpenGLWidgets Network)

# ── Dependencies (all vendored in third_party/) ──────────────────────────────

# ── GLM (header-only, vendored in third_party/) ─────────────────────────────
add_subdirectory(third_party/glm)

# ── Google Test + RapidCheck (vendored in third_party/) ──────────────────────
if(BUILD_TESTS)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    add_subdirectory(third_party/googletest)

    set(RC_ENABLE_GTEST ON CACHE BOOL "Build RapidCheck GTest integration" FORCE)
    add_subdirectory(third_party/rapidcheck)
endif()

# ── stb (header-only, vendored in third_party/) ─────────────────────────────
add_library(stb INTERFACE)
target_include_directories(stb INTERFACE ${CMAKE_SOURCE_DIR}/third_party)

# ── Subdirectories ───────────────────────────────────────────────────────────
add_subdirectory(src)

if(BUILD_TESTS)
    add_subdirectory(tests)
endif()
