
cmake_minimum_required(VERSION 3.25)
project(dantes_inferno LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT MSVC AND CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
    add_compile_options(-mssse3 -msse4.1)
endif()

include(generated/rexglue.cmake)

if(REXSDK_DIR)
    add_custom_command(
        OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/generated/default/codegen.build.stamp"
        APPEND
        DEPENDS
            "${REXSDK_DIR}/src/codegen/builders/vector.cpp"
            "${REXSDK_DIR}/include/rex/ppc/intrinsics.h"
    )
endif()

set(DANTESINFERNO_SOURCES
    src/main.cpp
)

if(WIN32)
    list(APPEND DANTESINFERNO_SOURCES src/dantes_inferno.rc)
    add_executable(dantes_inferno WIN32 ${DANTESINFERNO_SOURCES})
else()
    add_executable(dantes_inferno ${DANTESINFERNO_SOURCES})
endif()

rexglue_setup_target(dantes_inferno)

target_include_directories(dantes_inferno PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/rexglue-sdk/thirdparty/imgui"
)

if(TARGET imgui::imgui)
    target_link_libraries(dantes_inferno PRIVATE imgui::imgui)
elseif(TARGET imgui)
    target_link_libraries(dantes_inferno PRIVATE imgui)
endif()

option(DANTESINFERNO_NATIVE_RENDERER "Build native DiligentCore/Vulkan renderer" ON)

if(DANTESINFERNO_NATIVE_RENDERER)
    set(DILIGENT_CORE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/diligent-core"
        CACHE PATH "Path to DiligentCore source tree")

    if(NOT EXISTS "${DILIGENT_CORE_DIR}/CMakeLists.txt")
        message(FATAL_ERROR
            "DANTESINFERNO_NATIVE_RENDERER=ON but DiligentCore not found at:\n"
            "  ${DILIGENT_CORE_DIR}\n"
            "Clone it with:\n"
            "  git clone --depth 1 --recurse-submodules "
            "https://github.com/DiligentGraphics/DiligentCore.git "
            "thirdparty/diligent-core")
    endif()

    set(DILIGENT_NO_HLSL ON CACHE BOOL "Disable HLSL support in DiligentCore" FORCE)
    set(DILIGENT_INSTALL_CORE OFF CACHE BOOL "Don't install DiligentCore" FORCE)
    set(DILIGENT_BUILD_TESTS OFF CACHE BOOL "Don't build DiligentCore tests" FORCE)

    add_subdirectory("${DILIGENT_CORE_DIR}" "${CMAKE_BINARY_DIR}/diligent-core" EXCLUDE_FROM_ALL)

    add_library(dante_native_renderer STATIC
        src/native_renderer/dante_graphics_system.cpp
        src/native_renderer/native_device.cpp
        src/native_renderer/native_renderer_integration.cpp
        src/native_renderer/native_presenter.cpp
    )
    target_include_directories(dante_native_renderer PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/src/native_renderer"
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
    )
    target_include_directories(dante_native_renderer PRIVATE
        "${DILIGENT_CORE_DIR}"
        "${DILIGENT_CORE_DIR}/Graphics"
        "${DILIGENT_CORE_DIR}/Graphics/GraphicsEngine/interface"
        "${DILIGENT_CORE_DIR}/Graphics/GraphicsEngineVulkan/interface"
        "${DILIGENT_CORE_DIR}/Platforms/interface"
        "${DILIGENT_CORE_DIR}/Platforms/Win32/interface"
        "${DILIGENT_CORE_DIR}/Primitives/interface"
        "${DILIGENT_CORE_DIR}/Common/interface"
    )
    target_link_libraries(dante_native_renderer PRIVATE
        Diligent-GraphicsEngineVk-static
        Diligent-GraphicsTools
        Diligent-Common
        rex::runtime
        volk::volk
        Vulkan::Headers
    )

    target_link_libraries(dantes_inferno PRIVATE dante_native_renderer)
    target_compile_definitions(dantes_inferno PRIVATE DANTESINFERNO_NATIVE_RENDERER)

    if(WIN32)
        add_executable(native_renderer_test
            src/native_renderer/native_renderer_test.cpp
        )
        target_link_libraries(native_renderer_test PRIVATE
            dante_native_renderer
        )
        target_link_libraries(native_renderer_test PRIVATE user32 gdi32)
    endif()

    set(DANTESINFERNO_NATIVE_SOURCES
        src/native_renderer/dantes_inferno_native_main.cpp
    )
    if(WIN32)
        list(APPEND DANTESINFERNO_NATIVE_SOURCES src/dantes_inferno.rc)
        add_executable(dantes_inferno_native WIN32 ${DANTESINFERNO_NATIVE_SOURCES})
    else()
        add_executable(dantes_inferno_native ${DANTESINFERNO_NATIVE_SOURCES})
    endif()
    rexglue_setup_target(dantes_inferno_native)
    target_include_directories(dantes_inferno_native PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/rexglue-sdk/thirdparty/imgui"
    )
    target_link_libraries(dantes_inferno_native PRIVATE dante_native_renderer)
    target_compile_definitions(dantes_inferno_native PRIVATE DANTESINFERNO_NATIVE_RENDERER)

    message(STATUS "Native renderer (DiligentCore/Vulkan) enabled")
    message(STATUS "  Build native_renderer_test to verify clear+present")
    message(STATUS "  Build dantes_inferno_native for game with native renderer")
    message(STATUS "  dantes_inferno now links native presenter (use_native_presenter cvar)")
endif()
