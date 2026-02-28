enable_testing()

set(TEST_SOURCES
    placeholder_test.cpp
    test_vec3.cpp
    test_color.cpp
    test_image_texture.cpp
    test_skin_parser.cpp
    test_skin_parser_props.cpp
    test_mesh_builder.cpp
    test_mesh_builder_props.cpp
    test_intersection.cpp
    test_shading.cpp
    test_shading_props.cpp
    test_raytracer.cpp
    test_raytracer_props.cpp
    test_tile_renderer.cpp
    test_tile_renderer_props.cpp
    test_image_writer.cpp
    test_image_writer_props.cpp
    test_camera_controller.cpp
    test_camera_controller_props.cpp
)

add_executable(mcskin_tests ${TEST_SOURCES})

target_link_libraries(mcskin_tests PRIVATE
    mcskin_core
    GTest::gtest
    GTest::gtest_main
    rapidcheck
    rapidcheck_gtest
)

include(GoogleTest)
gtest_discover_tests(mcskin_tests DISCOVERY_MODE POST_BUILD)
