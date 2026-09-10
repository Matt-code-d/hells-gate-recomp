




set(REXSDK_VERSION "" CACHE STRING "Override SDK version (leave empty for default)")


set(REXSDK_DIR "" CACHE PATH "Path to rexglue-sdk source tree")
if(REXSDK_DIR)
    add_subdirectory("${REXSDK_DIR}" rexglue-sdk)
    message(STATUS "Using ReXGlue SDK from source tree: ${REXSDK_DIR}")
else()
    if(REXSDK_VERSION)
        find_package(rexglue ${REXSDK_VERSION} EXACT QUIET CONFIG)
    else()
        find_package(rexglue 0.10.0 QUIET CONFIG)
    endif()
    if(NOT rexglue_FOUND)
        message(FATAL_ERROR
            "ReXGlue SDK not found. Either:\n"
            "  - Set REXSDK_DIR to the rexglue-sdk source tree (e.g. thirdparty/rexglue-sdk)\n"
            "  - Install the SDK package and ensure it is on CMAKE_PREFIX_PATH")
    endif()
    message(STATUS "Found ReXGlue SDK ${REXGLUE_VERSION_STRING} at ${rexglue_DIR}")
endif()




if(NOT DEFINED REXGLUE_HOST_TARGET)
    set(REXGLUE_HOST_TARGET ${PROJECT_NAME})
endif()

set(REXGLUE_RECOMP_DEBUG_INFO "line-tables-only" CACHE STRING
    "Debug info level for generated code: line-tables-only, full, or none")



set(REXGLUE_RECOMP_OPTIONS "")
if(WIN32)
    if(MSVC)
        list(APPEND REXGLUE_RECOMP_OPTIONS /EHa)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        list(APPEND REXGLUE_RECOMP_OPTIONS -fasync-exceptions)
    endif()
endif()
if(REXGLUE_RECOMP_DEBUG_INFO STREQUAL "none")
    list(APPEND REXGLUE_RECOMP_OPTIONS
        $<$<CXX_COMPILER_ID:Clang,AppleClang,GNU>:-g0>)
elseif(REXGLUE_RECOMP_DEBUG_INFO STREQUAL "line-tables-only")
    list(APPEND REXGLUE_RECOMP_OPTIONS
        $<$<CXX_COMPILER_ID:Clang,AppleClang>:-gline-tables-only>)
endif()


if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/generated/default/sources.cmake")
    include(generated/default/sources.cmake)
    set(REXGLUE_ENTRYPOINT_GENERATED_SOURCES ${GENERATED_SOURCES})
    set(REXGLUE_ENTRYPOINT_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/generated/default")

    
    set_source_files_properties(${REXGLUE_ENTRYPOINT_GENERATED_SOURCES}
        PROPERTIES COMPILE_OPTIONS "${REXGLUE_RECOMP_OPTIONS}")
endif()




function(rexglue_apply_recomp_settings target_name pch_header)
    target_precompile_headers(${target_name} PRIVATE "${pch_header}")
    target_compile_options(${target_name} PRIVATE ${REXGLUE_RECOMP_OPTIONS})
endfunction()




macro(rexglue_setup_target target_name)
    if(REXGLUE_ENTRYPOINT_GENERATED_SOURCES)
        add_library(${target_name}_recomp OBJECT
            ${REXGLUE_ENTRYPOINT_GENERATED_SOURCES})
        target_include_directories(${target_name}_recomp PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}
            ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${REXGLUE_ENTRYPOINT_INCLUDE_DIR}
        )
        target_link_libraries(${target_name}_recomp PRIVATE rex::runtime)
        rexglue_apply_target_settings(${target_name}_recomp)
        rexglue_apply_recomp_settings(${target_name}_recomp
            "${REXGLUE_ENTRYPOINT_INCLUDE_DIR}/dantes_inferno_pch.h")
        add_dependencies(${target_name}_recomp dantes_inferno_codegen)
        target_link_libraries(${target_name} PRIVATE
            ${target_name}_recomp)
    endif()
    
    add_dependencies(${target_name} dantes_inferno_codegen)
    target_include_directories(${target_name} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${REXGLUE_ENTRYPOINT_INCLUDE_DIR}
    )
    target_link_libraries(${target_name} PRIVATE rex::runtime)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/metadata/icons")
        rexglue_embed_metadata(${target_name}
            DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/metadata/icons"
            PREFIX "icons")
    endif()
    rexglue_configure_target(${target_name} ${ARGN})
endmacro()







add_custom_command(
    OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/generated/default/codegen.build.stamp"
           ${REXGLUE_ENTRYPOINT_GENERATED_SOURCES}
    COMMAND $<TARGET_FILE:rex::rexglue> codegen ${CMAKE_CURRENT_SOURCE_DIR}/dantes_inferno_manifest.toml
    DEPFILE "${CMAKE_CURRENT_SOURCE_DIR}/generated/default/codegen.d"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Generating recompiled code for dantes_inferno"
    VERBATIM
)
add_custom_target(dantes_inferno_codegen
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/generated/default/codegen.build.stamp")


if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/generated/default/dll_targets.cmake")
    include(generated/default/dll_targets.cmake)
endif()
