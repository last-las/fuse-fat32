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

    class FAT32fs;

    class File {
    public:
        File(u32 parent_clus, u32 fst_entry_num, u32 fst_clus, FAT32fs &fs, std::string name,
             const fat32::ShortDirEntry *meta_entry) noexcept;

        /**
         * Return a null-terminated filename pointer.
         * */
        const char *name() noexcept;

        u64 read(byte *buf, u64 size, u64 offset) noexcept;

        u64 write(const byte *buf, u64 size, u64 offset) noexcept;

        void sync(bool sync_meta) noexcept;

        bool truncate(u32 length) noexcept;

        void setCrtTime(FatTimeStamp2 ts) noexcept;

        void setAccTime(FatTimeStamp ts) noexcept;

        void setWrtTime(FatTimeStamp ts) noexcept;

        virtual bool isDir() noexcept;

        // todo: merge markDeleted into renameTo.
        void markDeleted() noexcept;

        u64 ino() noexcept;

        // modify parent_sec_, meta_entry_num_ and filename to target's value
        void renameTo(shared_ptr<File> target) noexcept;

        /**
         * Read the nth sector of current file(the first sector is 0), or return nullptr when overflowed.
         * */
        std::optional<std::shared_ptr<device::Sector>> readSector(u32 n) noexcept;

        /**
         * Return the nth sector number of current file(the first sector is 0), or return nullptr when overflowed.
         * */
        std::optional<u32> sector_no(u32 n) noexcept;

        virtual ~File() noexcept;

    private:
        /**
         * Return the reference of the cluster chain, using a lazy strategy.
         * */
        std::vector<u32> &readClusChain() noexcept;

        /**
         * Iterate over each directory entry of the cluster chain, return false when chain_index exceeds.
         * */
        bool iterClusChainEntry(std::vector<u32> &clus_chain, u32 &chain_index, u32 &sec_index, u32 &sec_off) noexcept;

        fat32::FatTimeStamp2 crt_time_;
        fat32::FatDate acc_date_;
        fat32::FatTimeStamp wrt_time_;
        u32 file_sz_;

        /**
         * Custer number that contains the first directory entry of current file.
         * */
        u32 parent_clus_;
        /**
         * Current file's first directory entry number in parent cluster, start with zero.
         * */
        u32 fst_entry_num_;
        /**
         * The first sector number that contains current file's data.
         * */
        u32 fst_clus_;
        FAT32fs &fs_;
        std::string name_;
        bool is_deleted_ = false;
        /**
         * Possibly contains current file's cluster chain vector.
         * This field should never be used, use readClusChain() instead.
         * */
        std::optional<std::vector<u32>> clus_chain_;
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

        bool isDir() noexcept override;
    };

    class FAT32fs {
    public:
        // TODO: make the constructor private.
        FAT32fs(fat32::BPB bpb, fat32::FAT fat, device::Device &device) noexcept;

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

        fat32::BPB &bpb() noexcept;

        fat32::FAT &fat() noexcept;

        device::Device &device() noexcept;

    protected:
        fat32::BPB bpb_;
        fat32::FAT fat_;
        device::Device &device_;
    private:
        util::LRUCacheMap<u64, shared_ptr<File>> cached_lookup_files_{20};
        std::unordered_map<u64, shared_ptr<File>> cached_open_files_;
    };

} // namespace fs

#endif //STUPID_FAT32_FS_H
