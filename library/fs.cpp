#include "fs.h"

namespace fs {
    /**
     * File
     * */
    u64 File::read(u8 *buf, u64 size) noexcept {}

    u64 File::write(const u8 *buf, u64 size) noexcept {}

    void File::sync() noexcept {}

    bool File::truncate(u32 length) noexcept {}

    /**
     * Directory
     * */
    shared_ptr<File> Directory::crtFile(const char *name) noexcept {}

    shared_ptr<Directory> Directory::crtDir(const char *name) noexcept {}

    bool Directory::delFileEntry(const char *name) noexcept {}

    optional<shared_ptr<File>> Directory::lookupFile(const char *name) noexcept {}

    optional<shared_ptr<Directory>> Directory::lookupDir(const char *name) noexcept {}

    bool Directory::isEmpty() noexcept {}

    /**
     * Filesystem
     * */
    FAT32fs FAT32fs::from(device::Device* device) noexcept {
        return FAT32fs();
    }

    std::optional<FAT32fs> FAT32fs::mkfs(device::Device* device) noexcept {}

    shared_ptr<Directory> FAT32fs::getRootDir() noexcept {}

    // `getFile` and `getDir` will find the target on the opened file map
    std::optional<shared_ptr<File>> FAT32fs::getFile(u64 ino) noexcept {}

    std::optional<shared_ptr<Directory>> FAT32fs::getDir(u64 ino) noexcept {}

    void FAT32fs::markFileAsOpened(shared_ptr<fs::File> file) noexcept {}

    void FAT32fs::markFileAsClosed(shared_ptr<fs::File> file) noexcept {}
}