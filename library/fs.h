#ifndef STUPID_FAT32_FS_H
#define STUPID_FAT32_FS_H

#include <memory>
#include <optional>
#include "fat32.h"

namespace fs {
    using std::shared_ptr;
    using std::optional;
    using fat32::FatTimeStamp, fat32::FatTimeStamp2;

    class File {
    public:
        u64 read(u8 *buf, u64 size) noexcept;

        u64 write(const u8 *buf, u64 size) noexcept;

        void sync() noexcept;

        bool truncate(u32 length) noexcept;

        void setCrtTime(FatTimeStamp2 ts) noexcept {}

        void setAccTime(FatTimeStamp ts) noexcept {}

        void setWrtTime(FatTimeStamp ts) noexcept {}

        bool isDir() noexcept {}

        void markDeleted() noexcept {
            is_deleted_ = true;
        }

        u64 ino() noexcept {}

        // modify parent_sec_, entry_num_ and filename to target's value
        void renameTo(shared_ptr<File> target) noexcept {}

    private:
        u32 parent_sec_;
        u32 entry_num_;
        bool is_deleted_ = false;
    };

    class Directory : public File {
    public:
        shared_ptr<File> crtFile(const char *name) noexcept;

        shared_ptr<Directory> crtDir(const char *name) noexcept;

        bool delFileEntry(const char *name) noexcept;

        optional<shared_ptr<File>> lookupFile(const char *name) noexcept;

        optional<shared_ptr<Directory>> lookupDir(const char *name) noexcept;

        bool isEmpty() noexcept;
    };

    class FAT32fs {
    public:
        static FAT32fs from(device::Device *device) noexcept;

        static std::optional<FAT32fs> mkfs(device::Device *device) noexcept;

        shared_ptr<Directory> getRootDir() noexcept;

        optional<shared_ptr<File>> getFile(u64 ino) noexcept;

        optional<shared_ptr<Directory>> getDir(u64 ino) noexcept;
    };

} // namespace fs

#endif //STUPID_FAT32_FS_H
