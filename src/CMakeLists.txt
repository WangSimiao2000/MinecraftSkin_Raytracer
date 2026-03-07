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

# ── Renderer sources (renderer.h, cpu_renderer.h/.cpp added in later tasks) ──
file(GLOB RENDERER_SOURCES renderer/*.cpp)
list(APPEND CORE_SOURCES ${RENDERER_SOURCES})

# ── GpuRenderer stub (always compiled; provides fallback when Vulkan RT is off)
list(APPEND CORE_SOURCES vulkan/gpu_renderer.cpp)

# ── Vulkan GPU ray tracing sources (conditional) ─────────────────────────────
if(ENABLE_VULKAN_RT)
    file(GLOB VULKAN_SOURCES vulkan/*.cpp)
    # gpu_renderer.cpp is already added above; avoid duplicate
    list(REMOVE_ITEM VULKAN_SOURCES
         "${CMAKE_CURRENT_SOURCE_DIR}/vulkan/gpu_renderer.cpp")
    list(APPEND CORE_SOURCES ${VULKAN_SOURCES})
endif()

add_library(mcskin_core STATIC ${CORE_SOURCES})

target_include_directories(mcskin_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(mcskin_core PUBLIC
    glm::glm
    stb
)

# ── Link Vulkan when GPU ray tracing is enabled ──────────────────────────────
if(ENABLE_VULKAN_RT)
    target_link_libraries(mcskin_core PUBLIC Vulkan::Vulkan)

    # ── Compile ray tracing shaders to SPIR-V ────────────────────────────────
    find_program(GLSLC_EXECUTABLE glslc
        HINTS "$ENV{VULKAN_SDK}/Bin" "$ENV{VULKAN_SDK}/bin"
    )
    if(NOT GLSLC_EXECUTABLE)
        find_program(GLSLC_EXECUTABLE glslangValidator
            HINTS "$ENV{VULKAN_SDK}/Bin" "$ENV{VULKAN_SDK}/bin"
        )
        if(GLSLC_EXECUTABLE)
            set(GLSLC_IS_GLSLANG TRUE)
        endif()
    endif()

    if(NOT GLSLC_EXECUTABLE)
        message(FATAL_ERROR
            "Could not find glslc or glslangValidator.\n"
            "Please install the Vulkan SDK or ensure glslc is on your PATH.")
    endif()

    set(SHADER_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vulkan/shaders")
    set(SHADER_OUTPUT_DIR "${CMAKE_BINARY_DIR}/shaders")
    file(MAKE_DIRECTORY ${SHADER_OUTPUT_DIR})

    set(RT_SHADER_SOURCES
        ${SHADER_SOURCE_DIR}/raygen.rgen
        ${SHADER_SOURCE_DIR}/closesthit.rchit
        ${SHADER_SOURCE_DIR}/miss.rmiss
    )

    set(COMPILED_SHADERS "")
    foreach(SHADER_SRC ${RT_SHADER_SOURCES})
        get_filename_component(SHADER_NAME ${SHADER_SRC} NAME)
        set(SHADER_SPV "${SHADER_OUTPUT_DIR}/${SHADER_NAME}.spv")

        if(GLSLC_IS_GLSLANG)
            add_custom_command(
                OUTPUT ${SHADER_SPV}
                COMMAND ${GLSLC_EXECUTABLE}
                    -V --target-env vulkan1.2
                    -o ${SHADER_SPV}
                    ${SHADER_SRC}
                DEPENDS ${SHADER_SRC}
                COMMENT "Compiling shader ${SHADER_NAME} -> ${SHADER_NAME}.spv (glslangValidator)"
                VERBATIM
            )
        else()
            add_custom_command(
                OUTPUT ${SHADER_SPV}
                COMMAND ${GLSLC_EXECUTABLE}
                    --target-env=vulkan1.2
                    -o ${SHADER_SPV}
                    ${SHADER_SRC}
                DEPENDS ${SHADER_SRC}
                COMMENT "Compiling shader ${SHADER_NAME} -> ${SHADER_NAME}.spv (glslc)"
                VERBATIM
            )
        endif()

        list(APPEND COMPILED_SHADERS ${SHADER_SPV})
    endforeach()

    add_custom_target(compile_shaders DEPENDS ${COMPILED_SHADERS})
endif()

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

# ── Copy compiled shaders next to executable ─────────────────────────────────
if(ENABLE_VULKAN_RT)
    add_dependencies(mcskin_raytracer compile_shaders)

    foreach(SHADER_SPV ${COMPILED_SHADERS})
        get_filename_component(SPV_NAME ${SHADER_SPV} NAME)
        add_custom_command(TARGET mcskin_raytracer POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${SHADER_SPV}
                $<TARGET_FILE_DIR:mcskin_raytracer>/shaders/${SPV_NAME}
            COMMENT "Copying ${SPV_NAME} to output directory"
            VERBATIM
        )
    endforeach()
endif()

# ── Auto-deploy Qt DLLs on Windows after build ──────────────────────────────
if(WIN32)
    # Qt6 provides this helper since 6.5
    qt_generate_deploy_app_script(
        TARGET mcskin_raytracer
        OUTPUT_SCRIPT deploy_script
        NO_UNSUPPORTED_PLATFORM_ERROR
    )
    install(SCRIPT ${deploy_script})
endif()
