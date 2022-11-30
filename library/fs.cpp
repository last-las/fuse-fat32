#include <cstring>
#include <cassert>
#include <algorithm>
#include <memory>

#include "fs.h"
#include "fat32.h"

namespace fs {
    /**
     * File
     * */
    File::File(u32 parent_clus, u32 fst_entry_num, fs::FAT32fs &fs, std::string name,
               const fat32::ShortDirEntry *meta_entry) noexcept
            : parent_clus_{parent_clus}, fst_entry_num_{fst_entry_num}, fst_clus_{0}, fs_{fs},
              name_{std::move(name)}, crt_time_{meta_entry->crt_ts2}, acc_date_{meta_entry->lst_acc_date},
              wrt_time_{meta_entry->wrt_ts}, file_sz_{meta_entry->file_sz} {}

    const char *File::name() noexcept {
        return this->name_.c_str();
    }

    // todo: check `file_sz_`
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
        if (is_deleted_) {
            return;
        }
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
                auto *long_dir_entry = (const fat32::LongDirEntry *) sec->read_ptr(sec_off);
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
            fat32::setEntryClusNo(*short_dir_entry, fst_clus_);
        }

        // sync data info
        for (u32 n = 0; readSector(n).has_value(); ++n) {
            auto sector = readSector(n).value();
            sector->sync();
        }
    }

    // todo: The fst_clus might be 0x00!
    // todo: mark the dir_entry.ord 0x00 when remove some clusters...; change the file_sz_.
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

    void File::markDeleted() noexcept {
        // 1. delete dir entry occupied by current file
        auto p_clus_chain = fs_.fat().readClusChains(parent_clus_);
        u32 clus_i = 0;
        u32 sec_i = (fst_entry_num_ * sizeof(fat32::LongDirEntry)) / fs_.bpb().BPB_bytes_per_sec;
        u32 sec_off = (fst_entry_num_ * sizeof(fat32::LongDirEntry)) % fs_.bpb().BPB_bytes_per_sec;
        std::shared_ptr<device::Sector> sec;
        do {
            u32 sec_no = fat32::getFirstSectorOfCluster(fs_.bpb(), p_clus_chain[clus_i]) + sec_i;
            sec = fs_.device().readSector(sec_no).value();
            auto *dir_entry = (fat32::LongDirEntry *) sec->write_ptr(sec_off);
            u8 attr = dir_entry->attr;
            memset(dir_entry, 0x00, sizeof(fat32::LongDirEntry)); // todo: fix this.
            if ((attr & fat32::KAttrLongNameMask) != fat32::KAttrLongName) { // last dir entry is found
                break;
            }
        } while (iterClusChainEntry(p_clus_chain, clus_i, sec_i, sec_off));

        // 2. remove clus chain
        fs_.fat().freeClus(fst_clus_);
        is_deleted_ = true;
    }

    u64 File::ino() noexcept {
        return ((u64) parent_clus_ << 32) | fst_entry_num_;
    }

    void File::exchangeFstClus(shared_ptr<File> target) noexcept {
        // record current file's sz and fst_clus for exchange.
        u32 this_file_sz = file_sz_;
        u32 this_fst_clus = fst_clus_;

        // exchange
        this->file_sz_ = target->file_sz_;
        this->fst_clus_ = target->fst_clus_;
        target->file_sz_ = this_file_sz;
        target->fst_clus_ = this_fst_clus;

        this->sync(true);
        target->sync(true);
    }

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
    optional<shared_ptr<File>> Directory::crtFile(const char *name) noexcept {
        // convert the name to utf16 and calc required entry num
        util::string_utf8 utf8_name(name);
        util::string_utf16 utf16_name = util::utf8ToUtf16(utf8_name).value();
        u32 required_entry_num = (utf16_name.size() - 1) / fat32::KNameBytePerLongEntry + 1;
        required_entry_num++; // increase for extra short dir entry

        // try to find enough empty space
        u32 cur_entry;
        u32 free_entry_start = 0;
        u32 free_entry_cnt = 0;
        std::optional<fat32::LongDirEntry> result;
        for (cur_entry = 0;
             free_entry_cnt < required_entry_num && (result = readDirEntry(cur_entry)).has_value();
             cur_entry++) {
            auto dir_entry = result.value();
            if (fat32::isEmptyDirEntry(dir_entry)) {
                if (free_entry_cnt == 0) {
                    free_entry_start = cur_entry;
                }
                free_entry_cnt++;
            } else {
                free_entry_cnt = 0;
            }
        }

        // alloc more when not enough empty space
        if (free_entry_cnt < required_entry_num) {
            if (this->truncate(file_sz_ + (required_entry_num - free_entry_cnt) * sizeof(fat32::LongDirEntry))) {
                if (free_entry_cnt == 0) {
                    free_entry_start = cur_entry;
                }
            } else { // not enough disk space, return null
                return std::nullopt;
            }
        }

        // calculate CRC and generate basis-name of short dir entry
        fat32::BasisName basis_name = fat32::genBasisNameFrom(utf8_name);
        u8 chk_sum = fat32::chkSum(basis_name);
        u32 off = 0;

        for (u32 l_dir_ord = 1; l_dir_ord <= required_entry_num - 1; l_dir_ord++) {
            bool is_lst = (l_dir_ord == required_entry_num - 1);
            fat32::LongDirEntry l_dir_entry = fat32::mkLongDirEntry(is_lst, l_dir_ord, chk_sum, utf16_name, off);
            writeDirEntry(free_entry_start + required_entry_num - l_dir_ord - 1, l_dir_entry);
            off += fat32::KNameBytePerLongEntry;
        }
        const fat32::ShortDirEntry s_dir_entry = fat32::mkShortDirEntry(basis_name, false);
        writeDirEntry(free_entry_start + required_entry_num - 1, *(fat32::LongDirEntry *) (&s_dir_entry));

        return std::make_shared<File>(this->parent_clus_, free_entry_start,
                                      this->fs_, std::move(utf8_name), &s_dir_entry);
    }

    optional<shared_ptr<Directory>> Directory::crtDir(const char *name) noexcept {}

    bool Directory::delFileEntry(const char *name) noexcept {
        // 1. convert the name to utf16;
        // 2. traverse the content, try to find the name;
        // 3. remove all the dir entries related to name;
        // 4. determine whether to shrink the content.
    }

    optional<shared_ptr<File>> Directory::lookupFile(const char *name) noexcept {
        // 1. lookup name on cached lookup files.
    }

    optional<shared_ptr<File>> Directory::lookupFileByIndex(u32 index) noexcept {}


    bool Directory::isEmpty() noexcept {}

    bool Directory::isDir() noexcept {
        return true;
    }

    optional<fat32::LongDirEntry> Directory::readDirEntry(u32 n) noexcept {
        // todo
    }

    bool Directory::writeDirEntry(u32 n, fat32::LongDirEntry &dir_entry) noexcept {
        // todo
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