#ifndef STUPID_FAT32_FS_H
#define STUPID_FAT32_FS_H

#include <memory>
#include <optional>
#include <unordered_map>
#include "fat32.h"

namespace fs {
    using std::shared_ptr;
    using std::optional;
    using fat32::FatTimeStamp, fat32::FatTimeStamp2;

    class File {
    public:

        u64 read(byte *buf, u64 size, u64 offset) noexcept;

        /**
         * Return a null-terminated filename pointer.
         * */
        const char *name() noexcept;

        u64 write(const byte *buf, u64 size, u64 offset) noexcept;

        void sync(bool sync_meta) noexcept;

        bool truncate(u32 length) noexcept;

        void setCrtTime(FatTimeStamp2 ts) noexcept;

        void setAccTime(FatTimeStamp ts) noexcept;

        void setWrtTime(FatTimeStamp ts) noexcept;

        bool isDir() noexcept;

        void markDeleted() noexcept;

        u64 ino() noexcept;

        // modify parent_sec_, entry_num_ and filename to target's value
        void renameTo(shared_ptr<File> target) noexcept;

        virtual ~File() = default;

    private:
        u32 parent_sec_;
        u32 entry_num_;
        bool is_deleted_ = false;
    };

    class Directory : public File {
    public:
        // TODO: errno should also be returned.
        shared_ptr<File> crtFile(const char *name) noexcept;

        shared_ptr<Directory> crtDir(const char *name) noexcept;

        bool delFileEntry(const char *name) noexcept;

        optional<shared_ptr<File>> lookupFile(const char *name) noexcept;

        /**
         * Return the file ptr, or none if `index` is out of bound.
         * Usually this function is used to traverse the files in this directory.
         *
         * @param index the order of files in current directory
         * */
        optional<shared_ptr<File>> lookupFileByIndex(u32 index) noexcept;

        bool isEmpty() noexcept;
    };

    class FAT32fs {
    public:
        // TODO: make the constructor private.
        explicit FAT32fs(device::Device &device) noexcept;

        static FAT32fs from(device::Device &device) noexcept;

        static std::optional<FAT32fs> mkfs(device::Device *device) noexcept;

        shared_ptr<Directory> getRootDir() noexcept;

        optional<shared_ptr<File>> getFile(u64 ino) noexcept;

        optional<shared_ptr<Directory>> getDir(u64 ino) noexcept;

        /**
         * Get file from `cached_lookup_files_`, and store it in `cached_open_files`.
         * */
        optional<shared_ptr<File>> openFile(u64 ino) noexcept;

        /**
         * Remove file from `cached_open_files_`.
         * */
        void closeFile(u64 ino) noexcept;

    private:
        device::Device &device_;
        util::LRUCache<u64, shared_ptr<File>> cached_lookup_files_{20};
        std::unordered_map<u64, shared_ptr<File>> cached_open_files_;
    };

} // namespace fs

#endif //STUPID_FAT32_FS_H
