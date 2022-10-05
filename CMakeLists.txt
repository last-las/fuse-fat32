cmake_minimum_required(VERSION 3.10)
project(stupid-fat32)
set(CMAKE_CXX_STANDARD 17)

add_compile_options(-DFUSE_USE_VERSION=312 -DFUSERMOUNT_DIR="tmp")
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

add_executable(fat32_fuse programs/fat32_fuse.cpp library/fs.cpp)
target_include_directories(fat32_fuse PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/library/)
target_link_libraries(fat32_fuse PRIVATE fuse)