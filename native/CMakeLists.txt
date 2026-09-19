cmake_minimum_required(VERSION 3.20)
project(imux_native LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(imux_native SHARED
    cpp/imux_native.cpp
    c/imux_native_c.c
)
target_include_directories(imux_native PUBLIC include)
set_target_properties(imux_native PROPERTIES OUTPUT_NAME "imux_native")

if (WIN32)
    add_executable(ImuxLauncher WIN32
        cpp/ImuxLauncher.cpp
        cpp/imux_ui_engine.cpp
    )
    target_include_directories(ImuxLauncher PRIVATE include)
    target_link_libraries(ImuxLauncher PRIVATE d2d1 dwrite d3d11 d3dcompiler)
    target_compile_definitions(ImuxLauncher PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN)
    set_target_properties(ImuxLauncher PROPERTIES
        OUTPUT_NAME "ImuxLauncher"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin"
    )
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders/imux_ui.hlsl"
        "${CMAKE_BINARY_DIR}/bin/imux_ui.hlsl"
        COPYONLY
    )
endif()
