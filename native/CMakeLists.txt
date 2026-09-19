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
        "${CMAKE_CURRENT_SOURCE_DIR}/imux_game.rc.in"
        "${CMAKE_CURRENT_BINARY_DIR}/imux_game.rc"
        COPYONLY
    )
    file(COPY "${IMUX_GENERATED_ICON}" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")
    file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/imux_game.manifest" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")

    add_executable(ImuxGame WIN32
        cpp/ImuxGame.cpp
        cpp/imux_3d_engine.cpp
        "${CMAKE_CURRENT_BINARY_DIR}/imux_game.rc"
    )
    target_include_directories(ImuxGame PRIVATE include)
    target_link_libraries(ImuxGame PRIVATE d3d11 d3dcompiler dxgi windowscodecs ole32)
    target_compile_definitions(ImuxGame PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN)
    target_link_options(ImuxGame PRIVATE "/MANIFEST:NO")
    set_target_properties(ImuxGame PROPERTIES
        OUTPUT_NAME "ImuxGame"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin"
    )
    file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/assets" DESTINATION "${CMAKE_BINARY_DIR}/bin")
endif()
