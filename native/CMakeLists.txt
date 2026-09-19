cmake_minimum_required(VERSION 3.20)
project(imux_native VERSION 0.0.1 LANGUAGES C CXX)

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
    set(IMUX_GENERATED_ICON "${CMAKE_CURRENT_BINARY_DIR}/imux_icon.ico")
    if (NOT EXISTS "${IMUX_GENERATED_ICON}")
        message(FATAL_ERROR "Missing generated Imux icon: ${IMUX_GENERATED_ICON}. Run tools/python/convert_icon.py before configuring the native build.")
    endif()

    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/imux_launcher.rc.in"
        "${CMAKE_CURRENT_BINARY_DIR}/imux_launcher.rc"
        COPYONLY
    )
    file(COPY "${IMUX_GENERATED_ICON}" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")
    file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/imux_launcher.manifest" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")

    add_executable(ImuxLauncher WIN32
        cpp/ImuxLauncher.cpp
        cpp/imux_launcher_functions.cpp
        cpp/imux_3d_engine.cpp
        "${CMAKE_CURRENT_BINARY_DIR}/imux_launcher.rc"
    )
    target_include_directories(ImuxLauncher PRIVATE include)
    target_link_libraries(ImuxLauncher PRIVATE d2d1 dwrite d3d11 d3dcompiler windowscodecs ole32)
    target_compile_definitions(ImuxLauncher PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN)
    target_link_options(ImuxLauncher PRIVATE "/MANIFEST:NO")
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
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders/imux_world.hlsl"
        "${CMAKE_BINARY_DIR}/bin/imux_world.hlsl"
        COPYONLY
    )
    file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/assets" DESTINATION "${CMAKE_BINARY_DIR}/bin")
endif()
