























set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR AMD64)

if(NOT XWIN_SDK_PATH)
    if(DEFINED ENV{XWIN_SDK_PATH})
        set(XWIN_SDK_PATH "$ENV{XWIN_SDK_PATH}")
    else()
        set(XWIN_SDK_PATH "$ENV{HOME}/.local/opt/xwin-sdk")
    endif()
endif()

if(NOT EXISTS "${XWIN_SDK_PATH}/sdk/include/um")
    message(FATAL_ERROR
        "XWIN_SDK_PATH (${XWIN_SDK_PATH}) doesn't look like an xwin splat output "
        "(missing sdk/include/um). Run `xwin splat --output ${XWIN_SDK_PATH}` "
        "or pass -DXWIN_SDK_PATH=<path>.")
endif()

set(CMAKE_C_COMPILER clang-cl)
set(CMAKE_CXX_COMPILER clang-cl)
set(CMAKE_C_COMPILER_TARGET x86_64-pc-windows-msvc)
set(CMAKE_CXX_COMPILER_TARGET x86_64-pc-windows-msvc)
set(CMAKE_LINKER lld-link)
set(CMAKE_AR llvm-lib)


find_program(CMAKE_RC_COMPILER
    NAMES llvm-rc llvm-rc-22 llvm-rc-18
    HINTS "$ENV{HOME}/.local/opt/llvm-mingw/bin" /usr/lib64/llvm22/bin /usr/bin
)

















set(_xwin_libpath_flags
    "-libpath:\"${XWIN_SDK_PATH}/crt/lib/x86_64\" -libpath:\"${XWIN_SDK_PATH}/sdk/lib/um/x86_64\" -libpath:\"${XWIN_SDK_PATH}/sdk/lib/ucrt/x86_64\""
)
set(CMAKE_EXE_LINKER_FLAGS_INIT "/MANIFEST:NO /INCREMENTAL:NO ${_xwin_libpath_flags}")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "/MANIFEST:NO /INCREMENTAL:NO ${_xwin_libpath_flags}")
set(CMAKE_MODULE_LINKER_FLAGS_INIT "/MANIFEST:NO /INCREMENTAL:NO ${_xwin_libpath_flags}")

set(_xwin_includes
    "${XWIN_SDK_PATH}/crt/include"
    "${XWIN_SDK_PATH}/sdk/include/ucrt"
    "${XWIN_SDK_PATH}/sdk/include/um"
    "${XWIN_SDK_PATH}/sdk/include/shared"
    "${XWIN_SDK_PATH}/sdk/include/winrt"
    "${XWIN_SDK_PATH}/sdk/include/cppwinrt"
)
set(_xwin_flags "")
foreach(_inc ${_xwin_includes})
    string(APPEND _xwin_flags " -imsvc \"${_inc}\"")
endforeach()



set(CMAKE_C_FLAGS_INIT "-march=x86-64-v2${_xwin_flags}")





set(CMAKE_CXX_FLAGS_INIT "-march=x86-64-v2 /Zc:char8_t-${_xwin_flags}")






set(ENV{LIB} "${XWIN_SDK_PATH}/crt/lib/x86_64;${XWIN_SDK_PATH}/sdk/lib/um/x86_64;${XWIN_SDK_PATH}/sdk/lib/ucrt/x86_64")






set(CMAKE_FIND_ROOT_PATH "${XWIN_SDK_PATH}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
