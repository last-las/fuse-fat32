#include "fs.h"

namespace fs {
    /**
     * File
     * */
    u64 File::read(u8 *buf, u64 size) noexcept {}

    u64 File::write(const u8 *buf, u64 size) noexcept {}

    void File::sync() noexcept {}

    /**
     * Directory
     * */
    File Directory::crtFile(const char *name) noexcept {}

    Directory Directory::crtDir(const char *name) noexcept {}

    bool Directory::delFile(const char *name) noexcept {}

    bool Directory::delDir(const char *name) noexcept {}

    File Directory::lookupFile(const char *name) noexcept {}

    Directory Directory::lookupDir(const char *name) noexcept {}

    /**
     * Filesystem
     * */
    FAT32fs FAT32fs::from(device::Device device) noexcept {}

    std::optional<FAT32fs> FAT32fs::mkfs(device::Device) noexcept {}

    Directory FAT32fs::getRootDir() noexcept {}

    File FAT32fs::getFile(u64 ino) noexcept {}

    Directory FAT32fs::getDir(u64 ino) noexcept {}
}