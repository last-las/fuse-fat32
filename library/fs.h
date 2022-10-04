#ifndef STUPID_FAT32_FS_H
#define STUPID_FAT32_FS_H

#include <optional>
#include "fat32.h"

namespace fs {
    class File {
    public:
        u64 read(u8 *buf, u64 size) noexcept;

        u64 write(const u8 *buf, u64 size) noexcept;

        void sync() noexcept;
    };

    class Directory : public File {
    public:
        File crtFile(const char *name) noexcept;

        Directory crtDir(const char *name) noexcept;

        bool delFile(const char *name) noexcept;

        bool delDir(const char *name) noexcept;

        File lookupFile(const char *name) noexcept;

        Directory lookupDir(const char *name) noexcept;
    };

    class FAT32fs {
    public:
        static FAT32fs from(device::Device device) noexcept;

        static std::optional<FAT32fs> mkfs(device::Device) noexcept;

        Directory getRootDir() noexcept;

        File getFile(u64 ino) noexcept;

        Directory getDir(u64 ino) noexcept;
    };

} // namespace fs

#endif //STUPID_FAT32_FS_H
