cmake_minimum_required(VERSION 3.22)
project(MCSkin_RaytraceRenderer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# ── Qt6 ──────────────────────────────────────────────────────────────────────
find_package(Qt6 REQUIRED COMPONENTS Widgets OpenGL OpenGLWidgets)

# ── GLM ──────────────────────────────────────────────────────────────────────
find_package(glm REQUIRED)

# ── Google Test ──────────────────────────────────────────────────────────────
find_package(GTest REQUIRED)

# ── RapidCheck (FetchContent) ────────────────────────────────────────────────
include(FetchContent)
FetchContent_Declare(
    rapidcheck
    GIT_REPOSITORY https://github.com/emil-e/rapidcheck.git
    GIT_TAG        master
)
set(RC_ENABLE_GTEST ON CACHE BOOL "Build RapidCheck GTest integration" FORCE)
FetchContent_MakeAvailable(rapidcheck)

# ── stb (header-only, vendored in third_party/) ─────────────────────────────
add_library(stb INTERFACE)
target_include_directories(stb INTERFACE ${CMAKE_SOURCE_DIR}/third_party)

# ── Subdirectories ───────────────────────────────────────────────────────────
add_subdirectory(src)
add_subdirectory(tests)
