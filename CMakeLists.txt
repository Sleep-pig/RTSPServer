cmake_minimum_required(VERSION 3.15)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(RTSP LANGUAGES C CXX)

add_compile_options(-fstandalone-debug)
add_subdirectory(src/net)
add_subdirectory(src/logger)