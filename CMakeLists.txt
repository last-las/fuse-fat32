cmake_minimum_required(VERSION 3.16)
project(stupid-fat32)
set(CMAKE_CXX_STANDARD 17)
include(CheckSymbolExists)
include(CheckStructHasMember)
# todo:use 3.12 libfuse version

find_package(Iconv REQUIRED)

# gen config.h for libfuse(ref: libfuse/meson.build)
set(CMAKE_REQUIRED_DEFINITIONS -D_GNU_SOURCE)
set(INCLUDE_DEFAULT "stdio.h;stdlib.h;stddef.h;unistd.h;sys/types.h;sys/stat.h;fcntl.h;")
list(APPEND TEST_FUNCS fork fstatat openat readlinkat pipe2 splice vmsplice posix_fallocate fdatasync utimensat copy_file_range fallocate)
foreach (TEST_FUNC IN LISTS TEST_FUNCS)
    string(TOUPPER ${TEST_FUNC} UPPER_TEST_FUNC)
    check_symbol_exists(${TEST_FUNC} "${INCLUDE_DEFAULT}" HAVE_${UPPER_TEST_FUNC})
endforeach ()
check_symbol_exists(setxattr sys/xattr.h HAVE_SETXATTR)
check_symbol_exists(iconv iconv.h HAVE_ICONV)
check_struct_has_member("struct stat" st_atim "${INCLUDE_DEFAULT}" HAVE_STRUCT_STAT_ST_ATIM)
check_struct_has_member("struct stat" st_atimespec "${INCLUDE_DEFAULT}" HAVE_STRUCT_STAT_ST_ATIMESPEC)
configure_file(config.h.in config.h @ONLY)
unset(CMAKE_REQUIRED_DEFINITIONS)

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
target_include_directories(fuse PUBLIC libfuse/include libfuse/lib ${CMAKE_CURRENT_BINARY_DIR})

# fat32_fuse
add_executable(fat32_fuse programs/fat32_fuse.cpp library/fs.cpp library/device.cpp library/fat32.cpp library/util.cpp)
target_include_directories(fat32_fuse PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/library/)
target_link_libraries(fat32_fuse PRIVATE fuse pthread dl)

# test
add_subdirectory(googletest)
add_executable(test_system_call test/test_system_call.cpp)
target_link_libraries(test_system_call gtest gtest_main)

add_executable(test_utils_and_device test/test_utils_and_device.cpp library/device.cpp library/util.cpp) # todo: more simple one
target_link_libraries(test_utils_and_device gtest gtest_main)
target_include_directories(test_utils_and_device PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/library/ ${CMAKE_CURRENT_SOURCE_DIR}/test/)

add_executable(test_fat32 test/test_fat32.cpp library/device.cpp library/fs.cpp library/fat32.cpp library/util.cpp)
target_link_libraries(test_fat32 gtest gtest_main)
target_include_directories(test_fat32 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/library/ ${CMAKE_CURRENT_SOURCE_DIR}/test/)

add_executable(test_fs test/test_fs.cpp library/fs.cpp library/device.cpp library/fat32.cpp library/util.cpp)
target_link_libraries(test_fs gtest gtest_main)
target_include_directories(test_fs PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/library/ ${CMAKE_CURRENT_SOURCE_DIR}/test/)
