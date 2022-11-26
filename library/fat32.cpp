#include <cassert>

#include "fat32.h"

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

    std::string readShortEntryName(ShortDirEntry &short_dir_entry) {
        assert(parseEntryType(short_dir_entry) == EntryType::KExist);

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
        tm *tm_ = localtime(&unix_ts.tv_sec);
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

    u8 chkSum(const u8 *short_entry_name) {
        u8 sum = 0;
        for (short name_len = 11; name_len != 0; name_len--) {
            sum = ((sum & 1) ? 0x80 : 0) + (sum >> 1) + *short_entry_name++;
        }

        return sum;
    }

    util::string_gbk genShortNameFrom(util::string_utf8 utf8_str) {
        for(auto & c: utf8_str)
            c = std::toupper(c);

        util::strip(utf8_str, ' ');
        util::strip(utf8_str, '.');

        util::string_gbk bgk_str = util::utf8ToGbk(utf8_str).value();
    }

    /**
     * FAT implement
     * */
    FAT::FAT(BPB bpb, u32 free_count, device::Device &device) noexcept: bpb_{bpb}, device_{device} {
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

    bool FAT::resize(u32 fst_clus, u32 clus_num) noexcept {
        FATPos fat_pos;
        u32 pre_clus = fst_clus;
        u32 cur_clus = fst_clus;
        u32 clus_cnt = 0;
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

        if (clus_cnt < clus_num) { // alloc (clus_num - clus_cnt) sectors
            auto result = allocClus(clus_num - clus_cnt);
            if (!result.has_value()) {
                return false;
            }
            auto clus_chain = result.value();
            u32 alloc_fst_clus = clus_chain[0];
            fat_pos = getClusPosOnFAT(bpb_, pre_clus);
            writeFatEntry(fat_pos.fat_sec_num, fat_pos.fat_ent_offset, alloc_fst_clus);
        }

        return true;
    }

    void FAT::writeFatEntry(u32 sec_no, u32 fat_ent_offset, u32 val) noexcept {
        auto sector = this->device_.readSector(sec_no).value();
        writeFATClusEntryVal((u8 *) sector->write_ptr(0), fat_ent_offset, val);
    }

    u32 FAT::readFatEntry(u32 sec_no, u32 fat_ent_offset) noexcept {
        auto sector = this->device_.readSector(sec_no).value();
        return readFATClusEntryVal((const u8 *) sector->read_ptr(0), fat_ent_offset);
    }
}