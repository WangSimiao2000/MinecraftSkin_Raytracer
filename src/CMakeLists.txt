# Source files will be added as modules are implemented.
# For now, define the main executable target with a placeholder.

set(SOURCES
    main.cpp
    skin/stb_impl.cpp
    skin/image.cpp
    skin/skin_parser.cpp
    scene/mesh_builder.cpp
    raytracer/intersection.cpp
    raytracer/shading.cpp
    raytracer/raytracer.cpp
    raytracer/tile_renderer.cpp
    scene/camera.cpp
    output/image_writer.cpp
    gui/raster_preview.cpp
    gui/main_window.cpp
)

add_executable(mcskin_raytracer ${SOURCES})

target_include_directories(mcskin_raytracer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(mcskin_raytracer PRIVATE
    Qt6::Widgets
    Qt6::OpenGL
    Qt6::OpenGLWidgets
    glm::glm
    stb
)
