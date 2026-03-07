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
