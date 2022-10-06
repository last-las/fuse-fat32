#ifndef STUPID_FAT32_FS_H
#define STUPID_FAT32_FS_H

#include <memory>
#include <optional>
#include "fat32.h"

namespace fs {
    using std::shared_ptr;
    using std::optional;
    class File {
    public:
        u64 read(u8 *buf, u64 size) noexcept;

        u64 write(const u8 *buf, u64 size) noexcept;

        void sync() noexcept;

        bool truncate(u32 length) noexcept;

        u64 ino() noexcept {}
    };

    class Directory : public File {
    public:
        shared_ptr<File> crtFile(const char *name) noexcept;

        shared_ptr<File> crtDir(const char *name) noexcept;

        bool delFile(const char *name) noexcept;

        bool delDir(const char *name) noexcept;

        optional<shared_ptr<File>> lookupFile(const char *name) noexcept;

        optional<shared_ptr<Directory>> lookupDir(const char *name) noexcept;
    };

    class FAT32fs {
    public:
        static FAT32fs from(device::Device* device) noexcept;

        static std::optional<FAT32fs> mkfs(device::Device* device) noexcept;

        shared_ptr<Directory> getRootDir() noexcept;

        optional<shared_ptr<File>> getFile(u64 ino) noexcept;

        optional<shared_ptr<Directory>> getDir(u64 ino) noexcept;
    };

} // namespace fs

#endif //STUPID_FAT32_FS_H
