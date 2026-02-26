# Test target – test source files will be added as modules are implemented.

enable_testing()

# Placeholder test file so the target can be built immediately.
set(TEST_SOURCES
    placeholder_test.cpp
    test_vec3.cpp
    test_color.cpp
    test_image_texture.cpp
    test_skin_parser.cpp
    test_skin_parser_props.cpp
    test_mesh_builder_props.cpp
    test_mesh_builder.cpp
    test_intersection.cpp
    ${CMAKE_SOURCE_DIR}/src/skin/stb_impl.cpp
    ${CMAKE_SOURCE_DIR}/src/skin/image.cpp
    ${CMAKE_SOURCE_DIR}/src/skin/skin_parser.cpp
    ${CMAKE_SOURCE_DIR}/src/scene/mesh_builder.cpp
    ${CMAKE_SOURCE_DIR}/src/raytracer/intersection.cpp
    ${CMAKE_SOURCE_DIR}/src/raytracer/shading.cpp
    ${CMAKE_SOURCE_DIR}/src/raytracer/raytracer.cpp
    ${CMAKE_SOURCE_DIR}/src/raytracer/tile_renderer.cpp
    ${CMAKE_SOURCE_DIR}/src/scene/camera.cpp
    ${CMAKE_SOURCE_DIR}/src/output/image_writer.cpp
    test_image_writer.cpp
    test_image_writer_props.cpp
    test_shading.cpp
    test_shading_props.cpp
    test_raytracer.cpp
    test_raytracer_props.cpp
    test_tile_renderer.cpp
    test_tile_renderer_props.cpp
)

add_executable(mcskin_tests ${TEST_SOURCES})

target_include_directories(mcskin_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(mcskin_tests PRIVATE
    GTest::GTest
    GTest::Main
    rapidcheck
    rapidcheck_gtest
    glm::glm
    stb
)

include(GoogleTest)
gtest_discover_tests(mcskin_tests)
