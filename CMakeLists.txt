cmake_minimum_required(VERSION 3.25)
project(dantes_inferno LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT MSVC AND CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
    add_compile_options(-mssse3 -msse4.1)
endif()

include(generated/rexglue.cmake)

if(REXSDK_DIR)
    # The generated depfile only tracks the manifest and XEX. Track the ARM VMX
    # backend too so stale scalar code cannot survive an SDK/codegen update.
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

rexglue_setup_target(dantes_inferno GPU_PLUGINS xenos)

if(NOT MSVC AND CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|ARM64")
    # ARMv8.4-A is supported by the target SoC without relying on -mcpu=native,
    # which is unsafe under this device's userspace/toolchain combination.
    target_compile_options(dantes_inferno PRIVATE -march=armv8.4-a)
    if(TARGET dantes_inferno_recomp)
        target_compile_options(dantes_inferno_recomp PRIVATE -march=armv8.4-a)
    endif()

    # Optimize across the many generated recompilation translation units.
    include(CheckIPOSupported)
    check_ipo_supported(RESULT DANTESINFERNO_IPO_SUPPORTED OUTPUT DANTESINFERNO_IPO_ERROR)
    if(DANTESINFERNO_IPO_SUPPORTED)
        set_property(TARGET dantes_inferno PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
        if(TARGET dantes_inferno_recomp)
            set_property(TARGET dantes_inferno_recomp
                PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
        endif()
    else()
        message(WARNING "LTO unavailable for dantes_inferno: ${DANTESINFERNO_IPO_ERROR}")
    endif()
endif()

target_include_directories(dantes_inferno PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/rexglue-sdk/thirdparty/imgui"
)

# =============================================================================
# Native Renderer (DiligentCore/Vulkan) - IMPL-DC-001
# =============================================================================
# When enabled, builds the project-owned native renderer alongside the existing
# ReXGlue path. The existing build is unaffected when OFF (default).
option(DANTESINFERNO_NATIVE_RENDERER "Build native DiligentCore/Vulkan renderer" OFF)

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

    # DiligentCore sets global properties (USE_FOLDERS, etc.) and has its own
    # build settings. We add it as a subdirectory so its targets are available.
    # The Diligent-BuildSettings interface only applies to DiligentCore's own
    # targets, not ours, since we link privately.
    set(DILIGENT_NO_HLSL ON CACHE BOOL "Disable HLSL support in DiligentCore" FORCE)
    set(DILIGENT_INSTALL_CORE OFF CACHE BOOL "Don't install DiligentCore" FORCE)
    set(DILIGENT_BUILD_TESTS OFF CACHE BOOL "Don't build DiligentCore tests" FORCE)

    add_subdirectory("${DILIGENT_CORE_DIR}" "${CMAKE_BINARY_DIR}/diligent-core" EXCLUDE_FROM_ALL)

    # Native renderer library (project-owned, no DiligentCore types in headers)
    # Includes the integration module that bridges with ReXGlue's window system.
    add_library(dante_native_renderer STATIC
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

    # Link the native renderer into the main dantes_inferno target so the
    # D3D12 presenter can be replaced with the Vulkan presenter at runtime
    # (gated by the use_native_presenter cvar). The preprocessor definition
    # enables the conditional code in dantes_inferno_app.h.
    target_link_libraries(dantes_inferno PRIVATE dante_native_renderer)
    target_compile_definitions(dantes_inferno PRIVATE DANTESINFERNO_NATIVE_RENDERER)

    # Standalone test: creates a Win32 window, initializes native device,
    # and runs a clear+present loop. Does NOT need game assets.
    if(WIN32)
        add_executable(native_renderer_test
            src/native_renderer/native_renderer_test.cpp
        )
        target_link_libraries(native_renderer_test PRIVATE
            dante_native_renderer
        )
        # Need user32 for CreateWindow etc.
        target_link_libraries(native_renderer_test PRIVATE user32 gdi32)
    endif()

    # Native renderer build of the game: uses DantesInfernoNativeApp
    # (a derived class that adds native renderer hooks on top of the
    # original DantesInfernoApp). The original dantes_inferno target
    # and src/dantes_inferno_app.h are NOT modified.
    set(DANTESINFERNO_NATIVE_SOURCES
        src/native_renderer/dantes_inferno_native_main.cpp
    )
    if(WIN32)
        list(APPEND DANTESINFERNO_NATIVE_SOURCES src/dantes_inferno.rc)
        add_executable(dantes_inferno_native WIN32 ${DANTESINFERNO_NATIVE_SOURCES})
    else()
        add_executable(dantes_inferno_native ${DANTESINFERNO_NATIVE_SOURCES})
    endif()
    rexglue_setup_target(dantes_inferno_native GPU_PLUGINS xenos)
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

option(DANTESINFERNO_BUILD_LAUNCHER "Build the native Qt launcher" ON)
option(DANTESINFERNO_LAUNCHER_BUILD_TESTS "Build Qt launcher tests" ON)
if(DANTESINFERNO_BUILD_LAUNCHER AND UNIX AND NOT APPLE)
    find_package(Qt6 6.4 COMPONENTS Core Gui Widgets QUIET)
    if(Qt6_FOUND)
        if(DANTESINFERNO_LAUNCHER_BUILD_TESTS)
            enable_testing()
        endif()
        add_subdirectory(launcher-linux)
    else()
        message(WARNING
            "Qt 6.4+ was not found; dantes_inferno_launcher will not be built")
    endif()
endif()
