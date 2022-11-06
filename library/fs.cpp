#include "fs.h"
#include "fat32.h"

namespace fs {
    /**
     * File
     * */
    u64 File::read(byte *buf, u64 size, u64 offset) noexcept {}

    const char *File::name() noexcept {}

    u64 File::write(const byte *buf, u64 size, u64 offset) noexcept {}

    void File::sync(bool sync_meta) noexcept {}

    bool File::truncate(u32 length) noexcept {}

    void File::setCrtTime(FatTimeStamp2 ts) noexcept {}

    void File::setAccTime(FatTimeStamp ts) noexcept {}

    void File::setWrtTime(FatTimeStamp ts) noexcept {}

    bool File::isDir() noexcept {}

    void File::markDeleted() noexcept {}

    u64 File::ino() noexcept {}

    void File::renameTo(shared_ptr<File> target) noexcept {}

    /**
     * Directory
     * */
    shared_ptr<File> Directory::crtFile(const char *name) noexcept {}

    shared_ptr<Directory> Directory::crtDir(const char *name) noexcept {}

    bool Directory::delFileEntry(const char *name) noexcept {}

    optional<shared_ptr<File>> Directory::lookupFile(const char *name) noexcept {}

    optional<shared_ptr<File>> Directory::lookupFileByIndex(u32 index) noexcept {}


    bool Directory::isEmpty() noexcept {}

    /**
     * Filesystem
     * */
    FAT32fs::FAT32fs(device::Device &device) noexcept: device_{device} {}

    FAT32fs FAT32fs::from(device::Device &device) noexcept {
        auto fs = FAT32fs(device);
        auto fst_sec = fs.device_.readSector(0).value();
        fs.bpb = *((fat32::BPB *) fst_sec->read_ptr(0));
        if (!fat32::isValidFat32BPB(fs.bpb)) { // todo: use fuse log instead.
            printf("invalid BPB!\n");
            exit(1);
        }

        return fs;
    }

    std::optional<FAT32fs> FAT32fs::mkfs(device::Device *device) noexcept {}

    shared_ptr<Directory> FAT32fs::getRootDir() noexcept {
        u32 root_clus = this->bpb.BPB_root_clus;
        u32 sec_no_of_root_clus = fat32::getFirstSectorOfCluster(this->bpb, root_clus);
        // todo: read chains of clus.
        auto root_sec = this->device_.readSector(sec_no_of_root_clus).value();
    }

    // `getFile` and `getDir` will find the target on the opened file map
    std::optional<shared_ptr<File>> FAT32fs::getFile(u64 ino) noexcept {}

    std::optional<shared_ptr<Directory>> FAT32fs::getDir(u64 ino) noexcept {}

    optional<shared_ptr<File>> FAT32fs::openFile(u64 ino) noexcept {}

    void FAT32fs::closeFile(u64 ino) noexcept {}
}