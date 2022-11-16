#include <cstring>

#include "fs.h"
#include "fat32.h"

namespace fs {
    /**
     * File
     * */
    File::File(u32 parent_clus, u32 meta_entry_num, u32 fst_clus, fs::FAT32fs &fs, std::string name) noexcept:
            parent_clus_{parent_clus}, meta_entry_num_{meta_entry_num}, fst_clus_{fst_clus}, fs_{fs},
            name_{std::move(name)} {}

    const char *File::name() noexcept {
        return this->name_.c_str();
    }

    u64 File::read(byte *buf, u64 size, u64 offset) noexcept {
        fat32::BPB &bpb = fs_.bpb();
        u32 off_in_sec = offset / bpb.BPB_bytes_per_sec;
        u32 off_in_bytes = offset % bpb.BPB_bytes_per_sec;
        u32 read_sec_no = off_in_sec;
        if (!readSector(read_sec_no).has_value()) { // offset has exceeded the file size, return zero.
            return 0;
        }

        u64 remained_sz = size;
        byte *read_ptr = buf;
        auto sector = readSector(read_sec_no).value();
        u64 read_sz = std::min(remained_sz, (u64) (bpb.BPB_bytes_per_sec - off_in_bytes));
        memcpy(read_ptr, sector->read_ptr(off_in_bytes), read_sz);
        read_sec_no += 1;
        read_ptr += read_sz;
        remained_sz -= read_sz;
        for (; remained_sz > 0 && readSector(read_sec_no).has_value();
               read_sec_no++, read_ptr += read_sz, remained_sz -= read_sz) {
            sector = readSector(read_sec_no).value();
            read_sz = std::min((u64) bpb.BPB_bytes_per_sec, remained_sz);
            memcpy(read_ptr, sector->read_ptr(0), read_sz);
        }
        return read_ptr - buf;
    }

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

    std::optional<std::shared_ptr<device::Sector>> File::readSector(u32 n) noexcept {
        auto result = sector_no(n);
        if (result.has_value()) {
            return fs_.device().readSector(result.value());
        } else {
            return std::nullopt;
        }
    }

    std::optional<u32> File::sector_no(u32 n) noexcept {
        auto clus_chain = read_clus_chain();
        u32 sec_cnt = clus_chain.size() * fs_.bpb().BPB_sec_per_clus;
        if (n >= sec_cnt) { // overflowed, return empty
            return std::nullopt;
        }
        u32 clus_off = n / fs_.bpb().BPB_sec_per_clus;
        u32 sec_off = n % fs_.bpb().BPB_sec_per_clus;
        u32 fst_sec = fat32::getFirstSectorOfCluster(fs_.bpb(), clus_chain[clus_off]);
        return {fst_sec + sec_off};
    }

    std::vector<u32> &File::read_clus_chain() noexcept {
        if (!this->clus_chain_.has_value()) {
            this->clus_chain_ = fs_.fat().readClusChains(fst_clus_);
        }

        return this->clus_chain_.value();
    }

    /**
     * Directory
     * */
    shared_ptr<File> Directory::crtFile(const char *name) noexcept {}

    shared_ptr<Directory> Directory::crtDir(const char *name) noexcept {}

    bool Directory::delFileEntry(const char *name) noexcept {}

    optional<shared_ptr<File>> Directory::lookupFile(const char *name) noexcept {

    }

    optional<shared_ptr<File>> Directory::lookupFileByIndex(u32 index) noexcept {}


    bool Directory::isEmpty() noexcept {}

    /**
     * Filesystem
     * */
    FAT32fs::FAT32fs(fat32::BPB bpb, fat32::FAT fat, device::Device &device) noexcept
            : bpb_(bpb), fat_(fat), device_{device} {}

    FAT32fs FAT32fs::from(device::Device &device) noexcept {
        auto fst_sec = device.readSector(0).value();
        auto bpb = *((fat32::BPB *) fst_sec->read_ptr(0));
        if (!fat32::isValidFat32BPB(bpb)) { // todo: use fuse log instead.
            printf("invalid BPB!\n");
            exit(1);
        }

        auto fat = fat32::FAT(bpb, 0xffffffff, device);

        return {bpb, fat, device};
    }

    std::optional<FAT32fs> FAT32fs::mkfs(device::Device *device) noexcept {}

    shared_ptr<Directory> FAT32fs::getRootDir() noexcept {
        u32 root_clus = this->bpb_.BPB_root_clus;
        u32 sec_no_of_root_clus = fat32::getFirstSectorOfCluster(this->bpb_, root_clus);
        // todo: read chains of clus.
        auto root_sec = this->device_.readSector(sec_no_of_root_clus).value();
    }

    // `getFile` and `getDir` will find the target on the opened file map
    std::optional<shared_ptr<File>> FAT32fs::getFile(u64 ino) noexcept {}

    std::optional<shared_ptr<Directory>> FAT32fs::getDir(u64 ino) noexcept {}

    optional<shared_ptr<File>> FAT32fs::openFile(u64 ino) noexcept {}

    void FAT32fs::closeFile(u64 ino) noexcept {}

    fat32::BPB &FAT32fs::bpb() noexcept {
        return this->bpb_;
    }

    fat32::FAT &FAT32fs::fat() noexcept {
        return this->fat_;
    }

    device::Device &FAT32fs::device() noexcept {
        return this->device_;
    }
}