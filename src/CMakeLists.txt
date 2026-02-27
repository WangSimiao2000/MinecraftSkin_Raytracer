# ── Core library (non-GUI, shared between app and tests) ─────────────────────
set(CORE_SOURCES
    skin/stb_impl.cpp
    skin/image.cpp
    skin/skin_parser.cpp
    scene/mesh_builder.cpp
    scene/camera.cpp
    raytracer/intersection.cpp
    raytracer/shading.cpp
    raytracer/raytracer.cpp
    raytracer/tile_renderer.cpp
    output/image_writer.cpp
    gui/camera_controller.cpp
)

add_library(mcskin_core STATIC ${CORE_SOURCES})

target_include_directories(mcskin_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(mcskin_core PUBLIC
    glm::glm
    stb
)

# ── GUI executable ───────────────────────────────────────────────────────────
set(GUI_SOURCES
    main.cpp
    skin/skin_fetcher.cpp
    gui/raster_preview.cpp
    gui/main_window.cpp
)

add_executable(mcskin_raytracer ${GUI_SOURCES})

target_link_libraries(mcskin_raytracer PRIVATE
    mcskin_core
    Qt6::Widgets
    Qt6::OpenGL
    Qt6::OpenGLWidgets
    Qt6::Network
)
