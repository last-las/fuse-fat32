#ifndef STUPID_FAT32_FS_H
#define STUPID_FAT32_FS_H

#include <memory>
#include <optional>
#include <unordered_map>
#include "fat32.h"

namespace fs {
    using util::u8, util::u16, util::u32, util::i64, util::u64;
    using std::shared_ptr;
    using std::optional;
    using fat32::FatTimeStamp, fat32::FatTimeStamp2;

    const u64 KRootDirIno = 0;

    class FAT32fs;

    class File {
    public:
        File(u32 parent_clus, u32 fst_entry_num, FAT32fs &fs, std::string name,
             const fat32::ShortDirEntry *meta_entry) noexcept;

        static std::optional<std::shared_ptr<File>> fromIno(u64 ino, FAT32fs &fs) noexcept;

        /**
         * Return a null-terminated filename pointer.
         * */
        const char *name() noexcept;

        u32 read(char *buf, u32 size, u32 offset) noexcept;

        u32 write(const char *buf, u32 size, u32 offset) noexcept;

        void sync(bool sync_meta) noexcept;

        bool truncate(u32 length) noexcept;

        void setCrtTime(FatTimeStamp2 ts) noexcept;

        void setAccTime(FatTimeStamp ts) noexcept;

        void setWrtTime(FatTimeStamp ts) noexcept;

        fat32::FatTimeStamp2 crtTime() noexcept;

        fat32::FatTimeStamp accTime() noexcept;

        fat32::FatTimeStamp wrtTime() noexcept;

        virtual bool isDir() noexcept;

        // todo: rename the function name
        void markDeleted() noexcept;

        u64 ino() noexcept;

        /**
         * Exchange first cluster number and file size of the two files.
         * */
        void exchangeMetaData(shared_ptr<File> target) noexcept;

        /**
         * Read the nth sector of current file(the first sector is 0), or return nullptr when overflowed.
         * */
        std::optional<std::shared_ptr<device::Sector>> readSector(u32 n) noexcept;

        /**
         * Return the nth sector number of current file(the first sector is 0), or return nullptr when overflowed.
         * */
        std::optional<u32> sector_no(u32 n) noexcept;

        virtual u32 file_sz() noexcept;

        virtual ~File() noexcept;

    protected:
        /**
         * Return the reference of the cluster chain, using a lazy strategy.
         * */
        std::vector<u32> &readClusChain() noexcept;

        /**
         * Iterate over each directory entry of the cluster chain, return false when chain_index exceeds.
         * */
        static bool iterClusChainEntry(std::vector<u32> &clus_chain, u32 &chain_index, u32 &sec_index, u32 &sec_off,
                                       fat32::BPB &bpb) noexcept;

        u32 file_sz_;
        /**
         * Custer number that contains the first directory entry of current file.
         * */
        u32 parent_clus_;
        FAT32fs &fs_;
        /**
         * The creation time of the file, corresponding to linux "Change time".
         * */
        fat32::FatTimeStamp2 crt_time_;
        /**
         * The last time the file was read, corresponding to linux "Access time".
         * */
        fat32::FatDate acc_date_;
        /**
         * The last time the file was modified, corresponding to linux "Modify time".
         * */
        fat32::FatTimeStamp wrt_time_;
        /**
         * Current file's first directory entry number in parent cluster, start with zero.
         * */
        u32 fst_entry_num_;
        /**
         * The first sector number that contains current file's data.
         * */
        u32 fst_clus_;
        std::string name_;
        bool is_deleted_ = false;
        /**
         * Possibly contains current file's cluster chain vector.
         * This field should never be used, use readClusChain() instead.
         * */
        std::optional<std::vector<u32>> clus_chain_;
    };

    /**
     * Describe the range of a file's directory entries on parent directory.
     * */
    struct DirEntryRange {
        /**
         * Number of first directory entry on parent directory.
         * */
        u32 start;
        /**
         * The total counts of long directory entries and short directory entry.
         * */
        u32 count;
    };

    class Directory : public File {
    public:
        Directory(u32 parent_clus, u32 fst_entry_num, FAT32fs &fs, std::string name,
                  const fat32::ShortDirEntry *meta_entry) noexcept;

        static shared_ptr<Directory> mkRootDir(u32 fst_clus, FAT32fs &fs) noexcept;

        /**
         * Create an empty file and add to lru cache.
         * This function won't check whether the name exists, call `lookupFile` before if necessary.
         * */
        optional<shared_ptr<File>> crtFile(const char *name) noexcept;

        /**
         * Create an empty Directory and add to lru cache.
         * This function won't check whether the name exists, call `lookupFile` before if necessary.
         * */
        optional<shared_ptr<Directory>> crtDir(const char *name) noexcept;

        bool delFile(const char *name) noexcept;

        optional<shared_ptr<File>> lookupFile(const char *name) noexcept;

        /**
         * todo: alter comment
         * Return the file ptr, or none if `entry_off` is out of bound.
         * Usually this function is used to traverse the files in this directory.
         *
         * @param entry_off the order of files in current directory
         * */
        optional<shared_ptr<File>> lookupFileByIndex(u64 &entry_off) noexcept;

        bool isEmpty() noexcept;

        bool isDir() noexcept override;

        u32 file_sz() noexcept override;

    private:
        optional<shared_ptr<File>> crtFileInner(const char *name, bool is_dir) noexcept;

        optional<DirEntryRange> lookupFileInner(const char *name) noexcept;

        /**
         * Check whether the nth directory entry is the last non-empty entry by traversing the whole directory.
         * If n is less than zero, it returns true if the directory is empty.
         * */
        bool isLstNonEmptyEntry(i64 n) noexcept;

        /**
         * Read the nth directory entry, starting from zero.
         * */
        optional<fat32::LongDirEntry> readDirEntry(u32 n) noexcept;

        /**
         * Write the nth directory entry, starting from zero.
         * */
        bool writeDirEntry(u32 n, fat32::LongDirEntry &dir_entry) noexcept;
    };

    class FAT32fs {
    public:
        // TODO: make the constructor private.
        FAT32fs(fat32::BPB bpb, fat32::FAT fat, std::shared_ptr<device::Device> device) noexcept;

        static std::unique_ptr<FAT32fs> from(std::shared_ptr<device::Device> device) noexcept;

        static std::optional<FAT32fs> mkfs(std::shared_ptr<device::Device> device) noexcept;

        /**
         * Check whether the file name is valid before creating.
         *
         * Actually it is better to do validation inside Dir::crtFile and Dir::crtDir, however this requires us
         * to return different errno inside those functions, the std::expected applies here but it's in cpp standard 23.
         * */
        static bool isValidName(const char *name) noexcept;

        shared_ptr<Directory> getRootDir() noexcept;

        optional<shared_ptr<File>> getFileByIno(u64 ino) noexcept;

        // name should be utf8 encoded
        optional<shared_ptr<File>> getFileByName(const char *name) noexcept;

        optional<shared_ptr<Directory>> getDirByIno(u64 ino) noexcept;

        optional<shared_ptr<Directory>> getDirByName(const char *name) noexcept;

        void addFileToCache(shared_ptr<File> file) noexcept;

        /**
         * Remove the file object from cache by ino.
         * */
        void rmFileFromCacheByIno(u64 ino) noexcept;

        /**
         * Get file from `cached_lookup_files_`, and store it in `cached_open_files`.
         * */
        optional<shared_ptr<File>> openFile(u64 ino) noexcept;

        /**
         * Remove file from `cached_open_files_`.
         * */
        void closeFile(u64 ino) noexcept;

        void flush() noexcept;

        fat32::BPB &bpb() noexcept;

        fat32::FAT &fat() noexcept;

        std::shared_ptr<device::Device> device() noexcept;

    protected:
        fat32::BPB bpb_;
        fat32::FAT fat_;
        std::shared_ptr<device::Device> device_;
    private:
        util::LRUCacheMap<u64, shared_ptr<File>> cached_lookup_files_{20};
    };

} // namespace fs

#endif //STUPID_FAT32_FS_H
