cmake_minimum_required(VERSION 3.10)
project(stupid-fat32)
set(CMAKE_CXX_STANDARD 17)
# todo: solve the "fatal error: missing config.h"; search iconv

# libfuse
add_compile_options(-DFUSE_USE_VERSION=312 -DFUSERMOUNT_DIR="tmp" -ldl)
add_library(fuse STATIC
        libfuse/lib/buffer.c
        libfuse/lib/cuse_lowlevel.c
        libfuse/lib/fuse.c
        libfuse/lib/fuse_log.c
        libfuse/lib/fuse_loop.c
        libfuse/lib/fuse_loop_mt.c
        libfuse/lib/fuse_lowlevel.c
        libfuse/lib/fuse_opt.c
        libfuse/lib/fuse_signals.c
        libfuse/lib/helper.c
        libfuse/lib/mount.c
        libfuse/lib/mount_util.c
        libfuse/lib/modules/iconv.c
        libfuse/lib/modules/subdir.c
        )
target_include_directories(fuse PUBLIC libfuse/include libfuse/lib)

# fat32_fuse
add_executable(fat32_fuse programs/fat32_fuse.cpp library/fs.cpp library/device.cpp library/fat32.cpp library/util.cpp)
target_include_directories(fat32_fuse PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/library/)
target_link_libraries(fat32_fuse PRIVATE fuse pthread dl)

# test
add_subdirectory(googletest)
add_executable(test_system_call test/test_system_call.cpp)
target_link_libraries(test_system_call gtest gtest_main)

add_executable(test_utils test/test_utils.cpp library/device.cpp library/util.cpp) # todo: more simple one
target_link_libraries(test_utils gtest gtest_main)
target_include_directories(test_utils PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/library/ ${CMAKE_CURRENT_SOURCE_DIR}/test/)

add_executable(test_fs test/test_fs.cpp library/device.cpp library/fs.cpp library/fat32.cpp)
target_link_libraries(test_fs gtest gtest_main)
target_include_directories(test_fs PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/library/ ${CMAKE_CURRENT_SOURCE_DIR}/test/)
