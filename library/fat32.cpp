#include <cassert>
#include <cstring>
#include <time.h>
#include <sys/time.h>

#include "fat32.h"
#include "util.h"

// todo: move the platform dependencies to a separate file.
namespace fat32 {
    bool isValidFat32BPB(BPB &bpb) {
        // check BS_jmpBoot
        if ((bpb.BS_jmp_boot[0] != 0xEB || bpb.BS_jmp_boot[2] != 0x90)
            && bpb.BS_jmp_boot[0] != 0Xe9) {
            return false;
        }

        // FAT32 cluster cnt must be greater than 65525
        if (getCountOfClusters(bpb) < 65525) {
            return false;
        }

        // some field must be zero
        if (bpb.BPB_FATsz16 != 0) {
            return false;
        }

        return true;
    }

    FATPos getClusPosOnFAT(BPB &bpb, u32 n) {
        u32 fat_offset = n * 4;
        u32 this_fat_sec_num = bpb.BPB_resvd_sec_cnt + fat_offset / bpb.BPB_bytes_per_sec;
        u32 this_fat_ent_offset = fat_offset % bpb.BPB_bytes_per_sec;
        return FATPos{this_fat_sec_num, this_fat_ent_offset};
    }

    void writeFATClusEntryVal(u8 *sec_buff, u32 fat_ent_offset, u32 fat_clus_entry_val) {
        fat_clus_entry_val = fat_clus_entry_val & 0x0FFFFFFF;
        *((u32 *) &sec_buff[fat_ent_offset]) = (*((u32 *) &sec_buff[fat_ent_offset])) & 0xF0000000;
        *((u32 *) &sec_buff[fat_ent_offset]) = (*((u32 *) &sec_buff[fat_ent_offset])) | fat_clus_entry_val;
    }

    void assertFSInfo(FSInfo &fs_info) {
        assert(fs_info.lead_sig == KFsInfoLeadSig);
        assert(fs_info.struc_sig == KStrucSig);
        assert(fs_info.trail_sig == KTrailSig);
    }

    FSInfo makeFSInfo() {
        // todo
    }

    EntryType parseEntryType(ShortDirEntry &short_dir_entry) {
        u8 first_byte = (u8) short_dir_entry.name[0];
        if (first_byte == 0xE5) {
            return EntryType::KEmpty;
        } else if (first_byte == 0x00) {
            return EntryType::KEnd;
        } else {
            return EntryType::KExist;
        }
    }

    util::string_gbk readShortEntryName(ShortDirEntry &short_dir_entry) {
        assert(parseEntryType(short_dir_entry) == EntryType::KExist);
        util::string_gbk name;
        u8 c;
        // read primary
        for (int i = 0; i < 8; ++i) {
            if ((c = short_dir_entry.name[i]) == ' ') {
                break;
            }
            if (i == 0 && c == 0x05) {
                name.push_back((char) 0xe5);
                continue;
            }
            name.push_back((char) c);
        }
        // read (possible) extension
        for (int i = 8; i < 11; ++i) {
            if ((c = short_dir_entry.name[i]) == ' ') {
                break;
            }
            if (i == 8) {
                name.push_back('.');
            }
            name.push_back((char) c);
        }

        return name;
    }

    bool containIllegalShortDirEntryChr(u8 val) {
        if (val < 0x20 && val != 0x05) {
            return true;
        }
        for (auto inval_byte: KInvalidFatBytes) {
            if (val == inval_byte) {
                return true;
            }
        }

        return false;
    }

    FatTimeStamp unix2DosTs(timespec unix_ts) {
        // Input for both gmtime and localtime is UTC time stored in struct timespec, gmtime returns struct
        // expressed in UTC, but the return value of localtime is expressed relative to user's specified timezone.
        // On unix-like filesystem the time is always stored in UTC, so gmtime is used here.
        tm *tm_ = gmtime(&unix_ts.tv_sec);
        if (tm_->tm_sec == 60) { // ignore leap sec
            tm_->tm_sec = 59;
        }

        FatDate date = (((tm_->tm_year - 80) & 0b1111111) << 9) // bits 9-15: count of years from 1980
                       | (((tm_->tm_mon + 1) & 0b1111) << 5) // bits 5-8: month of year, 1 = January
                       | (tm_->tm_mday & 0b11111); // bits 0-4: day of month
        FatTime time = ((tm_->tm_hour & 0b11111) << 11) // bits 11-15: Hours
                       | ((tm_->tm_min & 0b111111) << 5) // bits 5-10: Minutes
                       | ((tm_->tm_sec / 2) & 0b11111); // bits 0-4: 2-second count
        return FatTimeStamp{time, date};
    }

    timespec dos2UnixTs(FatTimeStamp fat_ts) {
        tm tm_;
        tm_.tm_year = ((fat_ts.date >> 9) & 0b1111111) + 80;
        tm_.tm_mon = ((fat_ts.date >> 5) & 0b1111) - 1;
        tm_.tm_mday = fat_ts.date & 0b11111;
        tm_.tm_hour = (fat_ts.time >> 11) & 0b11111;
        tm_.tm_min = (fat_ts.time >> 5) & 0b111111;
        tm_.tm_sec = ((fat_ts.time) & 0b11111) * 2;
        tm_.tm_isdst = 0;

        time_t sec = mktime(&tm_);
        assert(sec != -1);
        return timespec{sec, 0};
    }

    FatTimeStamp2 unix2DosTs_2(timespec unix_ts) {
        FatTimeStamp fat_ts = unix2DosTs(unix_ts);
        u8 tenths_of_msec = (u8) (unix_ts.tv_nsec / 10000000);
        if (unix_ts.tv_sec % 2 == 1) {
            tenths_of_msec += 100;
        }
        return FatTimeStamp2{tenths_of_msec, fat_ts};
    }

    timespec dos2UnixTs_2(FatTimeStamp2 fat_ts2) {
        assert(fat_ts2.tt >= 0 && fat_ts2.tt <= 199);
        timespec unix_ts = dos2UnixTs(fat_ts2.ts);
        u8 tt = fat_ts2.tt;
        if (tt > 100) {
            unix_ts.tv_sec += 1;
            tt -= 100;
        }
        unix_ts.tv_nsec = tt * 10000000;
        return unix_ts;
    }

    FatTimeStamp getCurDosTs() noexcept {
        // clock_gettime(CLOCK_REALTIME, ...) and gettimeofday are the same in kernel.
        timespec unix_ts;
        assert(clock_gettime(CLOCK_REALTIME, &unix_ts) == 0);
        return fat32::unix2DosTs(unix_ts);
    }

    FatTimeStamp2 getCurDosTs2() noexcept {
        timespec unix_ts;
        assert(clock_gettime(CLOCK_REALTIME, &unix_ts) == 0);
        return fat32::unix2DosTs_2(unix_ts);
    }

    util::string_utf16 readLongEntryName(const LongDirEntry &long_dir_entry) {
        util::string_utf16 utf16_name;
        const u8 *name_ptr = &long_dir_entry.name1[0];
        for (u32 i = 0; i < 26; i += 2) {
            if ((name_ptr[0] == 0x00 && name_ptr[1] == 0x00) || (name_ptr[0] == 0xFF && name_ptr[1] == 0xFF)) {
                break;
            }
            utf16_name.push_back((char) name_ptr[0]);
            utf16_name.push_back((char) name_ptr[1]);
            name_ptr += 2;

            if (i + 2 == 10) {
                name_ptr = &long_dir_entry.name2[0];
            } else if (i + 2 == 22) {
                name_ptr = &long_dir_entry.name3[0];
            }
        }

        return utf16_name;
    }

    LongDirEntry mkLongDirEntry(bool is_lst, u8 ord, u8 chk_sum, util::string_utf16 &name, u32 off) {
        LongDirEntry long_dir_entry;
        long_dir_entry.ord = is_lst ? KLastLongEntry | ord : ord;
        long_dir_entry.attr = KAttrLongName;
        long_dir_entry.type = 0;
        long_dir_entry.chk_sum = chk_sum;
        long_dir_entry.fst_clus_low = 0;

        u32 name_len = name.length();
        u8 *name_ptr = &long_dir_entry.name1[0];
        for (int i = 1; i <= 13; i++) {
            if (off < name_len) {
                *name_ptr = name[off];
                *(name_ptr + 1) = name[off + 1];
            } else if (off == name_len) {
                *name_ptr = *(name_ptr + 1) = 0x00;
            } else { // start > name_len
                *name_ptr = *(name_ptr + 1) = 0xFF;
            }

            off += 2;
            name_ptr += 2;
            if (i == 5) {
                name_ptr = &long_dir_entry.name2[0];
            } else if (i == 5 + 6) {
                name_ptr = &long_dir_entry.name3[0];
            }
        }

        return long_dir_entry;
    }

    ShortDirEntry mkShortDirEntry(BasisName &basis_name, bool is_dir) {
        fat32::FatTimeStamp2 dos_ts2 = getCurDosTs2();
        fat32::FatTimeStamp dos_ts = dos_ts2.ts;

        ShortDirEntry shortDirEntry{0};
        memcpy(&shortDirEntry.name[0], &basis_name.primary[0], 8);
        memcpy(&shortDirEntry.name[8], &basis_name.extension[0], 3);
        shortDirEntry.attr = is_dir ? KAttrDirectory : 0;
        shortDirEntry.crt_ts2 = dos_ts2;
        shortDirEntry.lst_acc_date = dos_ts.date;
        shortDirEntry.wrt_ts = dos_ts;
        shortDirEntry.file_sz = 0;
        setFstClusNo(shortDirEntry, 0); // empty space

        return shortDirEntry;
    }

    u8 chkSum(BasisName &basis_name) {
        u8 sum = 0;
        char *name_ptr = &basis_name.primary[0];
        for (short name_len = 11; name_len != 0; name_len--) {
            sum = ((sum & 1) ? 0x80 : 0) + (sum >> 1) + *name_ptr++;
            if (name_ptr == &basis_name.primary[8]) {
                name_ptr = &basis_name.extension[0];
            }
        }

        return sum;
    }

    BasisName mkEmptyBasisName() {
        BasisName basis_name;
        memset(&basis_name, 0x20, sizeof(BasisName));
        basis_name.primary[8] = 0x00;
        basis_name.extension[3] = 0x00;
        return basis_name;
    }

    BasisName genBasisNameFromShort(util::string_gbk short_name) {
        u32 len = short_name.length();
        assert(len <= 12);
        BasisName basis_name = mkEmptyBasisName();
        int i;
        for (i = 0; i < 8 && i < len; i++) {
            if (short_name[i] == '.') {
                break;
            }
            basis_name.primary[i] = short_name[i];
        }

        if (i >= len) {
            return basis_name;
        } else {
            i++; // short_name[i] always eq '.', skip this dot.
            for (int j = 0; j < 3 && i < len; j++, i++) {
                basis_name.extension[j] = short_name[i];
            }
        }

        return basis_name;
    }

    BasisName genBasisNameFromLong(util::string_utf8 long_name) {
        // to upper case and strip ' ' & '.'
        util::toUpper(long_name);
        util::strip(long_name, ' ');
        util::strip(long_name, '.');

        // to gbk encoding
        util::string_gbk gbk_str = util::utf8ToGbk(long_name).value();
        BasisName basis_name = mkEmptyBasisName();

        // write primary portion
        for (int i = 0; i < 8 && i < gbk_str.length() && gbk_str[i] != '.'; i++) {
            basis_name.primary[i] = gbk_str[i];
        }

        // write extension portion if last period exists
        u32 last_dot_index = gbk_str.length();
        for (int i = int(gbk_str.length() - 1); i >= 0; i--) {
            if (gbk_str[i] == '.') {
                last_dot_index = i;
                break;
            }
        }
        for (u32 i = last_dot_index + 1, cnt = 0; cnt < 3 && i < gbk_str.length(); ++cnt, ++i) {
            basis_name.extension[cnt] = gbk_str[i];
        }

        return basis_name;
    }

    /**
     * FAT implement
     * */
    FAT::FAT(BPB bpb, u32 free_count, std::shared_ptr<device::Device> device) noexcept
            : bpb_{bpb}, device_{std::move(device)} {
        start_sec_no_ = getFirstFATSector(bpb, 0);
        fat_sec_num_ = bpb.BPB_FATsz32;
        cnt_of_clus_ = getCountOfClusters(bpb);
        free_count_ = free_count;
    }

    u64 FAT::availClusCnt() noexcept {
        if (this->avail_clus_cnt_.has_value()) {
            return this->avail_clus_cnt_.value();
        }

        u64 avail_clus_cnt = 0;
        u32 cnt_of_clus = 0;
        for (u32 i = this->start_sec_no_; i < this->end_sec_no() && cnt_of_clus < this->cnt_of_clus_; ++i) {
            for (u32 j = 0; j < SECTOR_SIZE / KFATEntSz && cnt_of_clus < this->cnt_of_clus_; ++j, ++cnt_of_clus) {
                if (readFatEntry(i, j * KFATEntSz) == 0) {
                    avail_clus_cnt += 1;
                }
            }
        }

        this->avail_clus_cnt_ = {avail_clus_cnt};
        return avail_clus_cnt;
    }

    void FAT::inc_avail_cnt(u64 no) noexcept {
        if (this->avail_clus_cnt_.has_value()) {
            this->avail_clus_cnt_.value() += no;
        }
    }

    void FAT::dec_avail_cnt(u64 no) noexcept {
        if (this->avail_clus_cnt_.has_value()) {
            this->avail_clus_cnt_.value() -= no;
        }
    }

    std::optional<std::vector<u32>> FAT::allocClus(u32 require_clus_num) noexcept {
        if (this->availClusCnt() < require_clus_num) {
            return std::nullopt;
        }

        FATPos fat_pos;
        u32 alloc_num = 0;
        u32 cnt_of_clus = 0;
        u32 pre_clus = 0, cur_clus = 0;
        std::vector<u32> clus_chain;
        if (this->free_count_ != 0xFFFFFFFF) {
            cur_clus = this->free_count_;
            clus_chain.push_back(cur_clus);
            this->free_count_ = 0xFFFFFFFF;
            dec_avail_cnt(1);
            alloc_num += 1;
            if (alloc_num == require_clus_num) {
                fat_pos = getClusPosOnFAT(bpb_, cur_clus);
                writeFatEntry(fat_pos.fat_sec_num, fat_pos.fat_ent_offset, KFat32EocMark);
                return {clus_chain};
            }
        }

        for (u32 i = this->start_sec_no_; i < this->end_sec_no() && cnt_of_clus < this->cnt_of_clus_; ++i) {
            for (u32 j = 0; j < SECTOR_SIZE / KFATEntSz && cnt_of_clus < this->cnt_of_clus_; ++j, ++cnt_of_clus) {
                if (readFatEntry(i, j * KFATEntSz) == 0) { // find
                    pre_clus = cur_clus;
                    cur_clus = cnt_of_clus;
                    if (pre_clus != 0) {
                        fat_pos = getClusPosOnFAT(bpb_, pre_clus);
                        writeFatEntry(fat_pos.fat_sec_num, fat_pos.fat_ent_offset, cur_clus);
                    }
                    clus_chain.push_back(cnt_of_clus);
                    dec_avail_cnt(1);
                    alloc_num += 1;
                    if (alloc_num == require_clus_num) {
                        fat_pos = getClusPosOnFAT(bpb_, cur_clus);
                        writeFatEntry(fat_pos.fat_sec_num, fat_pos.fat_ent_offset, KFat32EocMark);
                        return {clus_chain};
                    }
                }
            }
        }

        assert(false); // unreachable
    }

    void FAT::freeClus(u32 fst_clus) noexcept {
        u32 cur_clus = fst_clus;
        while (!isEndOfClusChain(cur_clus)) {
            FATPos fat_pos = getClusPosOnFAT(bpb_, cur_clus);
            cur_clus = readFatEntry(fat_pos.fat_sec_num, fat_pos.fat_ent_offset);
            writeFatEntry(fat_pos.fat_sec_num, fat_pos.fat_ent_offset, 0); // free
            inc_avail_cnt(1);
        }
    }

    std::vector<u32> FAT::readClusChains(u32 fst_clus) noexcept {
        std::vector<u32> clus_chains;
        u32 cur_clus = fst_clus;

        while (!isEndOfClusChain(cur_clus)) {
            clus_chains.push_back(cur_clus);
            FATPos fat_pos = getClusPosOnFAT(bpb_, cur_clus);
            cur_clus = readFatEntry(fat_pos.fat_sec_num, fat_pos.fat_ent_offset);
        }

        return clus_chains;
    }

    bool FAT::resize(u32 &fst_clus, u32 clus_num) noexcept {
        FATPos fat_pos;
        u32 pre_clus = fst_clus;
        u32 cur_clus = fst_clus;
        u32 clus_cnt = 0;
        if (isValidCluster(fst_clus)) {
            while (!isEndOfClusChain(cur_clus)) {
                pre_clus = cur_clus;
                fat_pos = getClusPosOnFAT(bpb_, cur_clus);
                cur_clus = readFatEntry(fat_pos.fat_sec_num, fat_pos.fat_ent_offset);
                clus_cnt++;
                if (clus_cnt > clus_num) { // free (clus_cnt - clus_num) sectors
                    fat_pos = getClusPosOnFAT(bpb_, pre_clus);
                    writeFatEntry(fat_pos.fat_sec_num, fat_pos.fat_ent_offset, 0);
                    dec_avail_cnt(1);
                } else if (clus_cnt == clus_num) {
                    fat_pos = getClusPosOnFAT(bpb_, pre_clus);
                    writeFatEntry(fat_pos.fat_sec_num, fat_pos.fat_ent_offset, KFat32EocMark);
                }
            }
        }

        if (clus_cnt < clus_num) { // alloc (clus_num - clus_cnt) sectors
            auto result = allocClus(clus_num - clus_cnt);
            if (!result.has_value()) {
                return false;
            }
            auto clus_chain = result.value();
            u32 alloc_fst_clus = clus_chain[0];
            if (!isValidCluster(fst_clus)) {
                fst_clus = alloc_fst_clus;
            } else {
                fat_pos = getClusPosOnFAT(bpb_, pre_clus);
                writeFatEntry(fat_pos.fat_sec_num, fat_pos.fat_ent_offset, alloc_fst_clus);
            }
        } else if (clus_num == 0) {
            fst_clus = 0;
        }

        return true;
    }

    void FAT::writeFatEntry(u32 sec_no, u32 fat_ent_offset, u32 val) noexcept {
        auto sector = this->device_->readSector(sec_no).value();
        writeFATClusEntryVal((u8 *) sector->write_ptr(0), fat_ent_offset, val);
    }

    u32 FAT::readFatEntry(u32 sec_no, u32 fat_ent_offset) noexcept {
        auto sector = this->device_->readSector(sec_no).value();
        return readFATClusEntryVal((const u8 *) sector->read_ptr(0), fat_ent_offset);
    }
}