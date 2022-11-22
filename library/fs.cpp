#include <cstring>
#include <cassert>
#include <algorithm>

#include "fs.h"
#include "fat32.h"

namespace fs {
    /**
     * File
     * */
    File::File(u32 parent_clus, u32 fst_entry_num, u32 fst_clus, fs::FAT32fs &fs, std::string name,
               const fat32::ShortDirEntry *meta_entry) noexcept
            : parent_clus_{parent_clus}, fst_entry_num_{fst_entry_num}, fst_clus_{fst_clus}, fs_{fs},
              name_{std::move(name)}, crt_time_{meta_entry->crt_ts2}, acc_date_{meta_entry->lst_acc_date},
              wrt_time_{meta_entry->wrt_ts}, file_sz_{meta_entry->file_sz} {}

    const char *File::name() noexcept {
        return this->name_.c_str();
    }

    u64 File::read(byte *buf, u64 size, u64 offset) noexcept {
        fat32::BPB &bpb = fs_.bpb();
        u32 off_in_sec = offset / bpb.BPB_bytes_per_sec;
        u32 off_in_bytes = offset % bpb.BPB_bytes_per_sec;
        u32 sec_no = off_in_sec;
        if (!readSector(sec_no).has_value()) { // offset has exceeded the file size, return zero.
            return 0;
        }

        u64 remained_sz = size;
        byte *wrt_ptr = buf;
        auto sector = readSector(sec_no).value();
        u64 wrt_sz = std::min(remained_sz, (u64) (bpb.BPB_bytes_per_sec - off_in_bytes));
        memcpy(wrt_ptr, sector->read_ptr(off_in_bytes), wrt_sz);
        sec_no += 1;
        wrt_ptr += wrt_sz;
        remained_sz -= wrt_sz;
        for (; remained_sz > 0 && readSector(sec_no).has_value();
               sec_no++, wrt_ptr += wrt_sz, remained_sz -= wrt_sz) {
            sector = readSector(sec_no).value();
            wrt_sz = std::min((u64) bpb.BPB_bytes_per_sec, remained_sz);
            memcpy(wrt_ptr, sector->read_ptr(0), wrt_sz);
        }
        return wrt_ptr - buf;
    }

    u64 File::write(const byte *buf, u64 size, u64 offset) noexcept {
        fat32::BPB &bpb = fs_.bpb();
        u32 off_in_sec = offset / bpb.BPB_bytes_per_sec;
        u32 off_in_bytes = offset % bpb.BPB_bytes_per_sec;
        u32 sec_no = off_in_sec;
        if (!readSector(sec_no).has_value()) { // offset exceeded, return.
            return 0;
        }

        u64 remained_sz = size;
        const byte *read_ptr = buf;
        auto sector = readSector(sec_no).value();
        u64 read_sz = std::min(remained_sz, (u64) (bpb.BPB_bytes_per_sec - off_in_bytes));
        memcpy(sector->write_ptr(off_in_bytes), read_ptr, read_sz);
        sec_no += 1;
        read_ptr += read_sz;
        remained_sz -= read_sz;
        for (; remained_sz > 0 && readSector(sec_no).has_value();
               sec_no++, read_ptr += read_sz, remained_sz -= read_sz) {
            sector = readSector(sec_no).value();
            read_sz = std::min((u64) bpb.BPB_bytes_per_sec, remained_sz);
            memcpy(sector->write_ptr(0), read_ptr, read_sz);
        }
        return read_ptr - buf;
    }

    void File::sync(bool sync_meta) noexcept {
        if (sync_meta) {
            auto p_clus_chain = fs_.fat().readClusChains(parent_clus_);
            u32 clus_i = 0;
            u32 sec_i = (fst_entry_num_ * sizeof(fat32::LongDirEntry)) / fs_.bpb().BPB_bytes_per_sec;
            u32 sec_off = (fst_entry_num_ * sizeof(fat32::LongDirEntry)) % fs_.bpb().BPB_bytes_per_sec;
            std::shared_ptr<device::Sector> sec;
            // find the short directory entry of current file in parent
            do {
                u32 sec_no = fat32::getFirstSectorOfCluster(fs_.bpb(), p_clus_chain[clus_i]) + sec_i;
                sec = fs_.device().readSector(sec_no).value();
                auto long_dir_entry = (const fat32::LongDirEntry *) sec->read_ptr(sec_off);
                if ((long_dir_entry->attr & fat32::KAttrLongNameMask) !=
                    fat32::KAttrLongName) { // find fst short dir entry
                    break;
                }
            } while (iterClusChainEntry(p_clus_chain, clus_i, sec_i, sec_off));

            // sync meta info
            auto short_dir_entry = (fat32::ShortDirEntry *) sec->write_ptr(sec_off);
            short_dir_entry->crt_ts2 = crt_time_;
            short_dir_entry->lst_acc_date = acc_date_;
            short_dir_entry->wrt_ts = wrt_time_;
            short_dir_entry->file_sz = file_sz_;
        }

        // sync data info
        for (u32 n = 0; readSector(n).has_value(); ++n) {
            auto sector = readSector(n).value();
            sector->sync();
        }
    }

    bool File::truncate(u32 length) noexcept {
        u32 clus_num = (length == 0 ? 0 : length - 1) / fat32::bytesPerClus(fs_.bpb()) + 1;
        if (fs_.fat().resize(fst_clus_, clus_num)) {
            clus_chain_ = std::nullopt;
            return true;
        }
        return false;
    }

    void File::setCrtTime(FatTimeStamp2 ts) noexcept {
        crt_time_ = ts;
    }

    void File::setAccTime(FatTimeStamp ts) noexcept {
        acc_date_ = ts.date;
    }

    void File::setWrtTime(FatTimeStamp ts) noexcept {
        wrt_time_ = ts;
    }

    bool File::isDir() noexcept {
        return false;
    }

    void File::markDeleted() noexcept {}

    u64 File::ino() noexcept {
        return ((u64) parent_clus_ << 32) | fst_entry_num_;
    }

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
        auto clus_chain = readClusChain();
        u32 sec_cnt = clus_chain.size() * fs_.bpb().BPB_sec_per_clus;
        if (n >= sec_cnt) { // overflowed, return empty
            return std::nullopt;
        }
        u32 clus_off = n / fs_.bpb().BPB_sec_per_clus;
        u32 sec_off = n % fs_.bpb().BPB_sec_per_clus;
        u32 fst_sec = fat32::getFirstSectorOfCluster(fs_.bpb(), clus_chain[clus_off]);
        return {fst_sec + sec_off};
    }

    std::vector<u32> &File::readClusChain() noexcept {
        if (!this->clus_chain_.has_value()) {
            this->clus_chain_ = fs_.fat().readClusChains(fst_clus_);
        }

        return this->clus_chain_.value();
    }

    bool File::iterClusChainEntry(std::vector<u32> &clus_chain, u32 &chain_index, u32 &sec_index,
                                  u32 &sec_off) noexcept {
        assert(sec_off < fs_.bpb().BPB_bytes_per_sec);

        sec_off += sizeof(fat32::LongDirEntry);
        if (sec_off == fs_.bpb().BPB_bytes_per_sec) {
            sec_off = 0;
            sec_index += 1;
            if (sec_index == fs_.bpb().BPB_sec_per_clus) {
                sec_index = 0;
                chain_index += 1;
                if (chain_index >= clus_chain.size()) {
                    return false;
                }
            }
        }
        return true;
    }

    File::~File() noexcept {
        sync(true);
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

    bool Directory::isDir() noexcept {
        return true;
    }

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