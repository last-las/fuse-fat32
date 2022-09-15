cmake_minimum_required(VERSION 3.10)
project(stupid-fat32)
set(CMAKE_CXX_STANDARD 14)

add_executable(fat32_fuse programs/fat32_fuse.cpp)
target_include_directories(fat32_fuse PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/libfuse/include)