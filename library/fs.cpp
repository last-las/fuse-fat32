#include <cstring>
#include <cassert>
#include <algorithm>
#include <memory>

#include "fs.h"
#include "fat32.h"

// todo: long directory is not checked(eg, chksum) for simplicity..
namespace fs {
    /**
     * File
     * */
    File::File(u32 parent_clus, u32 fst_entry_num, fs::FAT32fs &fs, std::string name,
               const fat32::ShortDirEntry *meta_entry) noexcept
            : parent_clus_{parent_clus}, fst_entry_num_{fst_entry_num}, fst_clus_{fat32::readEntryClusNo(*meta_entry)},
              fs_{fs}, name_{std::move(name)}, crt_time_{meta_entry->crt_ts2}, acc_date_{meta_entry->lst_acc_date},
              wrt_time_{meta_entry->wrt_ts}, file_sz_{meta_entry->file_sz} {}

    const char *File::name() noexcept {
        return this->name_.c_str();
    }

    u32 File::read(byte *buf, u32 size, u32 offset) noexcept {
        fat32::BPB &bpb = fs_.bpb();
        u32 off_in_sec = offset / bpb.BPB_bytes_per_sec;
        u32 off_in_bytes = offset % bpb.BPB_bytes_per_sec;
        u32 sec_no = off_in_sec;
        if (offset >= file_sz()) { // offset exceeds file size, return zero.
            return 0;
        }

        u32 remained_sz = std::min(size, file_sz() - offset);
        byte *wrt_ptr = buf;
        auto sector = readSector(sec_no).value();
        u32 wrt_sz = std::min(remained_sz, bpb.BPB_bytes_per_sec - off_in_bytes);
        memcpy(wrt_ptr, sector->read_ptr(off_in_bytes), wrt_sz);
        sec_no += 1;
        wrt_ptr += wrt_sz;
        remained_sz -= wrt_sz;
        for (; remained_sz > 0 && readSector(sec_no).has_value();
               sec_no++, wrt_ptr += wrt_sz, remained_sz -= wrt_sz) {
            sector = readSector(sec_no).value();
            wrt_sz = std::min((u32) bpb.BPB_bytes_per_sec, remained_sz);
            memcpy(wrt_ptr, sector->read_ptr(0), wrt_sz);
        }

        setAccTime(fat32::getCurDosTs());
        return wrt_ptr - buf;
    }

    u32 File::write(const byte *buf, u32 size, u32 offset) noexcept {
        fat32::BPB &bpb = fs_.bpb();
        u32 off_in_sec = offset / bpb.BPB_bytes_per_sec;
        u32 off_in_bytes = offset % bpb.BPB_bytes_per_sec;
        u32 sec_no = off_in_sec;
        if (offset + size >= file_sz() &&
            !truncate(offset + size)) { // offset exceeds file size, try to expand size first
            return 0;
        }

        u32 remained_sz = size;
        const byte *read_ptr = buf;
        auto sector = readSector(sec_no).value();
        u32 read_sz = std::min(remained_sz, bpb.BPB_bytes_per_sec - off_in_bytes);
        memcpy(sector->write_ptr(off_in_bytes), read_ptr, read_sz);
        sec_no += 1;
        read_ptr += read_sz;
        remained_sz -= read_sz;
        for (; remained_sz > 0 && readSector(sec_no).has_value();
               sec_no++, read_ptr += read_sz, remained_sz -= read_sz) {
            sector = readSector(sec_no).value();
            read_sz = std::min((u32) bpb.BPB_bytes_per_sec, remained_sz);
            memcpy(sector->write_ptr(0), read_ptr, read_sz);
        }
        setWrtTime(fat32::getCurDosTs());
        return read_ptr - buf;
    }

    // todo: add synced flag
    // todo: directory and file will write to the same area, this might introduce problems.
    void File::sync(bool sync_meta) noexcept {
        if (is_deleted_) {
            return;
        }
        if (sync_meta && ino() != KRootDirIno) { // never try to sync root directory metadata
            auto p_clus_chain = fs_.fat().readClusChains(parent_clus_);
            u32 clus_i = 0;
            u32 sec_i = (fst_entry_num_ * sizeof(fat32::LongDirEntry)) / fs_.bpb().BPB_bytes_per_sec;
            u32 sec_off = (fst_entry_num_ * sizeof(fat32::LongDirEntry)) % fs_.bpb().BPB_bytes_per_sec;
            std::shared_ptr<device::Sector> sec;
            // find the short directory entry of current file in parent
            do {
                u32 sec_no = fat32::getFirstSectorOfCluster(fs_.bpb(), p_clus_chain[clus_i]) + sec_i;
                sec = fs_.device()->readSector(sec_no).value();
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
            if (!isDir()) {
                short_dir_entry->file_sz = file_sz_;
            }
            fat32::setFstClusNo(*short_dir_entry, fst_clus_);
        }

        // sync data info
        for (u32 n = 0; readSector(n).has_value(); ++n) {
            auto sector = readSector(n).value();
            sector->sync();
        }
    }

    bool File::truncate(u32 length) noexcept {
        u32 clus_num = length == 0 ? 0 : ((length - 1) / fat32::bytesPerClus(fs_.bpb()) + 1);
        if (fs_.fat().resize(fst_clus_, clus_num, isDir())) {
            clus_chain_ = std::nullopt;
            if (!isDir()) {
                file_sz_ = length;
            }
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
        if (is_deleted_ || ino() == KRootDirIno) {
            return;
        }

        // delete dir entry occupied by current file
        auto p_clus_chain = fs_.fat().readClusChains(parent_clus_);
        u32 clus_i = 0;
        u32 sec_i = (fst_entry_num_ * sizeof(fat32::LongDirEntry)) / fs_.bpb().BPB_bytes_per_sec;
        u32 sec_off = (fst_entry_num_ * sizeof(fat32::LongDirEntry)) % fs_.bpb().BPB_bytes_per_sec;
        std::shared_ptr<device::Sector> sec;
        do {
            u32 sec_no = fat32::getFirstSectorOfCluster(fs_.bpb(), p_clus_chain[clus_i]) + sec_i;
            sec = fs_.device()->readSector(sec_no).value();
            auto *dir_entry = (fat32::LongDirEntry *) sec->write_ptr(sec_off);
            u8 attr = dir_entry->attr;
            fat32::setDirEntryEmpty(*dir_entry);
            if ((attr & fat32::KAttrLongNameMask) != fat32::KAttrLongName) { // last dir entry is found
                break;
            }
        } while (iterClusChainEntry(p_clus_chain, clus_i, sec_i, sec_off));

        // remove clus chain
        fs_.fat().freeClus(fst_clus_);

        // remove from fs cache
        fs_.rmFileFromCacheByName(name());
        is_deleted_ = true;
    }

    u64 File::ino() noexcept {
        return ((u64) parent_clus_ << 32) | fst_entry_num_;
    }

    void File::exchangeMetaData(shared_ptr<File> target) noexcept {
        // record current file's meta info for exchange.
        auto this_file_sz = file_sz_;
        auto this_fst_clus = fst_clus_;
        auto this_crt_time = crt_time_;
        auto this_acc_date = acc_date_;
        auto this_wrt_time = wrt_time_;

        // exchange
        this->file_sz_ = target->file_sz_;
        this->fst_clus_ = target->fst_clus_;
        this->crt_time_ = target->crt_time_;
        this->acc_date_ = target->acc_date_;
        this->wrt_time_ = target->wrt_time_;
        target->file_sz_ = this_file_sz;
        target->fst_clus_ = this_fst_clus;
        target->crt_time_ = this_crt_time;
        target->acc_date_ = this_acc_date;
        target->wrt_time_ = this_wrt_time;

        this->sync(true);
        target->sync(true);
    }

    std::optional<std::shared_ptr<device::Sector>> File::readSector(u32 n) noexcept {
        auto result = sector_no(n);
        if (result.has_value()) {
            return fs_.device()->readSector(result.value());
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

    u32 File::file_sz() noexcept {
        return file_sz_;
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
    Directory::Directory(u32 parent_clus, u32 fst_entry_num, FAT32fs &fs, std::string name,
                         const fat32::ShortDirEntry *meta_entry) noexcept
            : File(parent_clus, fst_entry_num, fs, std::move(name), meta_entry) {}

    shared_ptr<Directory> Directory::mkRootDir(u32 fst_clus, FAT32fs &fs) noexcept {
        fat32::ShortDirEntry s_dir_entry{};
        fat32::setFstClusNo(s_dir_entry, fst_clus);
        return std::make_shared<Directory>(0, 0, fs, "/", &s_dir_entry);
    }

    optional<shared_ptr<File>> Directory::crtFile(const char *name) noexcept {
        return crtFileInner(name, false);
    }

    optional<shared_ptr<Directory>> Directory::crtDir(const char *name) noexcept {
        auto result = crtFileInner(name, true);
        if (result.has_value()) {
            auto dir_obj = std::dynamic_pointer_cast<Directory>(result.value());
            // alloc a cluster
            auto bytes_per_clus = fat32::bytesPerClus(fs_.bpb());
            if (!dir_obj->truncate(bytes_per_clus)) { // no enough space
                assert(delFile(name));
                return std::nullopt;
            }
            // create dot and dotdot entries
            auto dot_entry = fat32::mkDotShortDirEntry(crt_time_, dir_obj->fst_clus_);
            auto dot_dot_entry = fat32::mkDotDotShortDirEntry(crt_time_, fst_clus_);
            assert(dir_obj->writeDirEntry(0, fat32::castShortDirEntryToLong(dot_entry)));
            assert(dir_obj->writeDirEntry(1, fat32::castShortDirEntryToLong(dot_dot_entry)));
            return {dir_obj};
        } else {
            return std::nullopt;
        }
    }

    bool Directory::delFile(const char *name) noexcept {
        auto result = lookupFileInner(name);
        if (!result.has_value()) {
            return false;
        }
        DirEntryRange range = result.value();

        // remove all the dir entries related to target name
        u32 cur_no;
        for (cur_no = range.start; cur_no < range.start + range.count; cur_no++) {
            fat32::LongDirEntry dir_entry{};
            fat32::setDirEntryEmpty(dir_entry);
            writeDirEntry(cur_no, dir_entry); // inefficient but easy to understand...
        }

        u32 short_dir_entry_no = range.start + range.count - 1;
        if (isLstNonEmptyEntry(short_dir_entry_no)) {
            // mark the short dir entry last empty
            fat32::LongDirEntry dir_entry{};
            fat32::setDirEntryLstEmpty(dir_entry);
            writeDirEntry(short_dir_entry_no, dir_entry);

            // shrink the Directory size
            truncate((range.start + 1) * fat32::KDirEntrySize);
        }

        fs_.rmFileFromCacheByName(name);
        return true;
    }

    optional<shared_ptr<File>> Directory::lookupFile(const char *name) noexcept {
        auto cache_result = fs_.getFileByName(name);
        if (cache_result.has_value()) {
            return cache_result;
        }

        auto lookup_result = lookupFileInner(name);
        if (!lookup_result.has_value()) {
            return std::nullopt;
        }
        DirEntryRange range = lookup_result.value();
        fat32::LongDirEntry lst_dir_entry = readDirEntry(range.start + range.count - 1).value();
        fat32::ShortDirEntry s_dir_entry = fat32::castLongDirEntryToShort(lst_dir_entry);

        shared_ptr<File> file;
        if (fat32::isDirectory(s_dir_entry)) {
            file = std::make_shared<Directory>(this->fst_clus_, range.start, this->fs_, name, &s_dir_entry);
        } else {
            file = std::make_shared<File>(this->fst_clus_, range.start, this->fs_, name, &s_dir_entry);
        }
        this->fs_.addFileToCache(file);
        return file;
    }

    optional<shared_ptr<File>> Directory::lookupFileByIndex(u64 &entry_off) noexcept {
        std::optional<fat32::LongDirEntry> result;
        fat32::LongDirEntry l_dir_entry{};
        fat32::ShortDirEntry s_dir_entry{};
        u32 entry_start = 0, entry_cnt = 0;

        // skip possible empty entries
        while ((result = readDirEntry(entry_off)).has_value() && fat32::isEmptyDirEntry(result.value())) {
            entry_off++;
        }

        if (!result.has_value() || fat32::isLstEmptyDirEntry(result.value())) {
            // reach the end of directory, return null
            return std::nullopt;
        }

        entry_start = entry_off;

        // record all the entries until a short entry is found
        std::list<fat32::LongDirEntry> l_dir_entries;
        while (true) {
            result = readDirEntry(entry_off);
            assert(result.has_value());
            l_dir_entry = result.value();
            assert(!fat32::isEmptyDirEntry(l_dir_entry));
            entry_cnt++;
            entry_off++;
            if (!fat32::isLongDirEntry(l_dir_entry)) { // a short dir entry is found, break here
                s_dir_entry = fat32::castLongDirEntryToShort(l_dir_entry);
                break;
            }
            l_dir_entries.push_front(l_dir_entry);
        }


        util::string_gbk short_name = fat32::readShortEntryName(s_dir_entry);
        util::string_utf8 name;
        if (!l_dir_entries.empty()) {
            auto basis_name = fat32::genBasisNameFromShort(short_name);
            u8 chk_sum = fat32::chkSum(basis_name);

            util::string_utf16 utf16_name;
            for (const auto &dir_entry: l_dir_entries) {
                assert(fat32::isValidLongDirEntry(dir_entry, chk_sum));
                utf16_name += fat32::readLongEntryName(dir_entry);
            }
            name = util::utf16ToUtf8(utf16_name).value();
        } else {
            name = util::gbkToUtf8(short_name).value();
        }

        auto cache_result = fs_.getFileByName(name.c_str());
        if (cache_result.has_value()) {
            return cache_result;
        }

        shared_ptr<File> file;
        if (fat32::isDirectory(s_dir_entry)) {
            file = std::make_shared<Directory>(this->fst_clus_, entry_start, this->fs_, std::move(name), &s_dir_entry);
        } else {
            file = std::make_shared<File>(this->fst_clus_, entry_start, this->fs_, std::move(name), &s_dir_entry);
        }
        this->fs_.addFileToCache(file);
        return file;
    }

    bool Directory::isEmpty() noexcept {
        return isLstNonEmptyEntry(-1);
    }

    bool Directory::isDir() noexcept {
        return true;
    }

    u32 Directory::file_sz() noexcept {
        return readClusChain().size() * fat32::bytesPerClus(fs_.bpb());
    }

    optional<shared_ptr<File>> Directory::crtFileInner(const char *name, bool is_dir) noexcept {
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

        // alloc more when there is not enough empty space
        if (free_entry_cnt < required_entry_num) {
            if (this->truncate(file_sz() + (required_entry_num - free_entry_cnt) * sizeof(fat32::LongDirEntry))) {
                if (free_entry_cnt == 0) {
                    free_entry_start = cur_entry;
                }
            } else { // not enough disk space, return null
                return std::nullopt;
            }
        }

        // calculate CRC and generate basis-name of short dir entry
        fat32::BasisName basis_name = fat32::genBasisNameFromLong(utf8_name);
        u8 chk_sum = fat32::chkSum(basis_name);
        u32 off = 0;

        // write the dir entries onto the disk, create and return a File object.
        for (u32 l_dir_ord = 1; l_dir_ord <= required_entry_num - 1; l_dir_ord++) {
            bool is_lst = (l_dir_ord == required_entry_num - 1);
            fat32::LongDirEntry l_dir_entry = fat32::mkLongDirEntry(is_lst, l_dir_ord, chk_sum, utf16_name, off);
            writeDirEntry(free_entry_start + required_entry_num - l_dir_ord - 1, l_dir_entry);
            off += fat32::KNameBytePerLongEntry;
        }
        const fat32::ShortDirEntry s_dir_entry = fat32::mkShortDirEntry(basis_name, is_dir);
        writeDirEntry(free_entry_start + required_entry_num - 1, *(fat32::LongDirEntry *) (&s_dir_entry));

        shared_ptr<File> file;
        if (is_dir) {
            file = std::make_shared<Directory>(this->fst_clus_, free_entry_start,
                                               this->fs_, std::move(utf8_name), &s_dir_entry);
        } else {
            file = std::make_shared<File>(this->fst_clus_, free_entry_start,
                                          this->fs_, std::move(utf8_name), &s_dir_entry);
        }

        this->fs_.addFileToCache(file);
        return file;
    }

    optional<DirEntryRange> Directory::lookupFileInner(const char *name) noexcept {
        util::string_utf8 utf8_name(name);
        util::string_utf16 utf16_name = util::utf8ToUtf16(utf8_name).value();
        util::toUpper(utf8_name);
        util::string_gbk gbk_name = util::utf8ToGbk(utf8_name).value();

        // traverse entries, try to find target name
        bool is_find = false, has_l_entry = false;
        u32 target_entry_start, target_entry_cnt = 0;
        u8 chk_sum;
        util::string_utf16 read_long_name;
        std::optional<fat32::LongDirEntry> result;
        fat32::LongDirEntry l_dir_entry{};
        fat32::ShortDirEntry s_dir_entry{};
        for (u32 cur_entry = 0; (result = readDirEntry(cur_entry)).has_value(); cur_entry++) {
            l_dir_entry = result.value();
            if (fat32::isLstEmptyDirEntry(l_dir_entry)) {
                break;
            }
            if (fat32::isEmptyDirEntry(l_dir_entry)) {
                has_l_entry = false;
                target_entry_cnt = 0;
                read_long_name = "";
                continue;
            }

            if (target_entry_cnt == 0) {
                target_entry_start = cur_entry;
                chk_sum = l_dir_entry.chk_sum;
            }
            target_entry_cnt++;

            if (!fat32::isLongDirEntry(l_dir_entry)) { // find a short dir entry, judge whether it's the target
                s_dir_entry = fat32::castLongDirEntryToShort(l_dir_entry);
                auto read_short_name = fat32::readShortEntryName(s_dir_entry);
                auto basis_name = fat32::genBasisNameFromShort(read_short_name);

                assert(fat32::chkSum(basis_name) == chk_sum);

                if ((has_l_entry && read_long_name == utf16_name)
                    || (!has_l_entry && read_short_name == gbk_name)) { // the lookup target is found
                    is_find = true;
                    break;
                }

                // not the target name, continue reading
                has_l_entry = false;
                target_entry_cnt = 0;
                read_long_name = "";
            } else { // find a long dir entry, record the name part
                assert(fat32::isValidLongDirEntry(l_dir_entry, chk_sum));
                has_l_entry = true;
                read_long_name.insert(0, fat32::readLongEntryName(l_dir_entry));
            }
        }

        if (!is_find) {
            return std::nullopt;
        } else {
            return DirEntryRange{target_entry_start, target_entry_cnt};
        }
    }

    bool Directory::isLstNonEmptyEntry(i64 n) noexcept {
        n = n < 0 ? 0 : n + 1;
        optional<fat32::LongDirEntry> result;
        while ((result = readDirEntry(n)).has_value()) {
            auto dir_entry = result.value();
            if (!fat32::isEmptyDirEntry(dir_entry)) {
                return false;
            }
            if (fat32::isLstEmptyDirEntry(dir_entry)) {
                return true;
            }
        }

        return true;
    }

    optional<fat32::LongDirEntry> Directory::readDirEntry(u32 n) noexcept {
        fat32::LongDirEntry l_dir_entry{};
        u32 entry_sz = sizeof(fat32::LongDirEntry);
        u32 rd_sz = this->read((byte *) &l_dir_entry, entry_sz, entry_sz * n);
        if (rd_sz == entry_sz) {
            return {l_dir_entry};
        } else {
            return std::nullopt;
        }
    }

    bool Directory::writeDirEntry(u32 n, fat32::LongDirEntry &dir_entry) noexcept {
        u32 entry_sz = sizeof(fat32::LongDirEntry);
        u32 wrt_sz = this->write((const byte *) &dir_entry, entry_sz, entry_sz * n);
        return wrt_sz == entry_sz;
    }

    /**
     * Filesystem
     * */
    FAT32fs::FAT32fs(fat32::BPB bpb, fat32::FAT fat, std::shared_ptr<device::Device> device) noexcept
            : bpb_(bpb), fat_(fat), device_{std::move(device)} {}

    std::unique_ptr<FAT32fs> FAT32fs::from(std::shared_ptr<device::Device> device) noexcept {
        auto fst_sec = device->readSector(0).value();
        auto bpb = *((fat32::BPB *) fst_sec->read_ptr(0));
        if (!fat32::isValidFat32BPB(bpb)) { // todo: use fuse log instead.
            printf("invalid BPB!\n");
            exit(1);
        }

        auto fat = fat32::FAT(bpb, 0xffffffff, device);

        return std::make_unique<FAT32fs>(bpb, fat, std::move(device));
    }

    std::optional<FAT32fs> FAT32fs::mkfs(std::shared_ptr<device::Device> device) noexcept {
        return std::nullopt;
    }

    shared_ptr<Directory> FAT32fs::getRootDir() noexcept {
        u32 root_fst_clus = this->bpb_.BPB_root_clus;
        return Directory::mkRootDir(root_fst_clus, *this);
    }

    std::optional<shared_ptr<File>> FAT32fs::getFileByIno(u64 ino) noexcept {
        assert(false);
    }

    optional<shared_ptr<File>> FAT32fs::getFileByName(const char *name) noexcept {
        for (const auto &ino_file: cached_lookup_files_) {
            auto file = ino_file.second;
            if (strcmp(file->name(), name) == 0) {
                return {file};
            }
        }

        return std::nullopt;
    }

    std::optional<shared_ptr<Directory>> FAT32fs::getDirByIno(u64 ino) noexcept {
        assert(false);
    }

    optional<shared_ptr<Directory>> FAT32fs::getDirByName(const char *name) noexcept {
        assert(false);
    }

    void FAT32fs::addFileToCache(shared_ptr<File> file) noexcept {
        u64 ino = file->ino();
        this->cached_lookup_files_.put(ino, std::move(file));
    }

    void FAT32fs::rmFileFromCacheByName(const char *name) noexcept {
        auto result = getFileByName(name);
        if (result.has_value()) {
            auto file = result.value();
            cached_lookup_files_.remove(file->ino());
        }
    }

    optional<shared_ptr<File>> FAT32fs::openFile(u64 ino) noexcept {
        assert(false);
    }

    void FAT32fs::closeFile(u64 ino) noexcept {
        assert(false);
    }

    void FAT32fs::flush() noexcept {
        this->cached_lookup_files_.clear();
        this->device_->clear();
    }

    fat32::BPB &FAT32fs::bpb() noexcept {
        return this->bpb_;
    }

    fat32::FAT &FAT32fs::fat() noexcept {
        return this->fat_;
    }

    std::shared_ptr<device::Device> FAT32fs::device() noexcept {
        return this->device_;
    }
}