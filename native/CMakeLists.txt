cmake_minimum_required(VERSION 3.20)
project(ImuxGame LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT WIN32)
    message(FATAL_ERROR "ImuxGame is Windows-only.")
endif()

add_executable(ImuxGame WIN32
    cpp/ImuxGame.cpp
    cpp/imux_3d_engine.cpp
)

target_include_directories(ImuxGame PRIVATE include)
target_compile_definitions(ImuxGame PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(ImuxGame PRIVATE d3d11 d3dcompiler)

set_target_properties(ImuxGame PROPERTIES
    OUTPUT_NAME "ImuxGame"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin"
)
