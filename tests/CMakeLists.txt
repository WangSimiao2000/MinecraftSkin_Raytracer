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
    test_pose_control_props.cpp
    test_pose_control.cpp
    test_cpu_renderer.cpp
    test_device_capability.cpp
    test_gpu_renderer.cpp
    test_acceleration_structure_props.cpp
)

# GUI sources needed for GUI tests (not part of mcskin_core)
set(GUI_TEST_SOURCES
    ${CMAKE_SOURCE_DIR}/src/skin/skin_fetcher.cpp
    ${CMAKE_SOURCE_DIR}/src/gui/raster_preview.cpp
    ${CMAKE_SOURCE_DIR}/src/gui/main_window.cpp
)

add_executable(mcskin_tests ${TEST_SOURCES} ${GUI_TEST_SOURCES})

target_link_libraries(mcskin_tests PRIVATE
    mcskin_core
    GTest::gtest
    GTest::gtest_main
    rapidcheck
    rapidcheck_gtest
    Qt6::Widgets
    Qt6::OpenGL
    Qt6::OpenGLWidgets
    Qt6::Network
)

include(GoogleTest)
gtest_discover_tests(mcskin_tests DISCOVERY_MODE PRE_TEST)
