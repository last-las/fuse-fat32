#ifndef STUPID_FAT32_FAT32_H
#define STUPID_FAT32_FAT32_H

#include <ctime>

#include "device.h"
#include "util.h"

/**
 * For simplicity, Fat8/16 is not supported here.
 * */

// todo: organize the structure of this file.
namespace fat32 {
    const u32 KFat32EocMark = 0x0FFFFFF8;
    const u32 KClnShutBitMask = 0x08000000;
    const u32 KHrdErrBitMask = 0x04000000;
    const u32 KFATEntSz = sizeof(u32);
    // FsInfo
    const u32 KFsInfoLeadSig = 0x41615252;
    const u32 KStrucSig = 0x61417272;
    const u32 KTrailSig = 0xAA550000;
    // Short Directory Entry
    const u8 KDirEntrySize = 32;
    const u8 KAttrReadOnly = 0x01;
    const u8 KAttrHidden = 0x02;
    const u8 KAttrSystem = 0x04;
    const u8 KAttrVolumeID = 0x08;
    const u8 KAttrDirectory = 0x10;
    const u8 KAttrArchive = 0x20;
    const u8 KInvalidFatBytes[] = {0x22, 0x2A, 0x2B, 0x2C, 0x2E, 0x2F, 0x3A, 0x3B,
                                   0x3C, 0x3D, 0x3E, 0x3F, 0x5B, 0x5C, 0x5D, 0x7c};
    // Long Directory Entry
    const u8 KAttrLongName = KAttrReadOnly | KAttrHidden | KAttrSystem | KAttrVolumeID;
    const u8 KAttrLongNameMask =
            KAttrReadOnly | KAttrHidden | KAttrSystem | KAttrVolumeID | KAttrDirectory | KAttrArchive;
    const u8 KLastLongEntry = 0x40;
    const u8 KNameBytePerLongEntry = 26;

    struct BPB {
        u8 BS_jmp_boot[3];
        char BS_oem_name[8];
        u16 BPB_bytes_per_sec;
        u8 BPB_sec_per_clus;
        u16 BPB_resvd_sec_cnt;
        u8 BPB_num_fats;
        u16 BPB_root_ent_cnt;
        u16 BPB_tot_sec16;
        u8 BPB_media;
        u16 BPB_FATsz16;
        u16 BPB_sec_per_trk;
        u16 BPB_num_heads;
        u32 BPB_hidd_sec;
        u32 BPB_tot_sec32;
        u32 BPB_FATsz32;
        u16 BPB_ext_flags;
        u16 BPB_FSVer;
        u32 BPB_root_clus;
        u16 BPB_fs_info;
        u16 BPB_bk_boot_sec;
        u8 BPB_reserved[12];
        u8 BS_drv_num;
        u8 BS_reserved1;
        u8 BS_boot_sig;
        u32 BS_vol_id;
        char BS_vol_lab[11];
        char BS_fil_sys_type[8];
    } __attribute__((packed));

    // todo: check CountOfClusters >= 65525
    bool isValidFat32BPB(BPB &bpb);

    // todo: make sure that CountOfClusters >= 65525
    BPB makeFat32BPB();

    inline u32 getFirstFATSector(BPB &bpb, u32 n) {
        return bpb.BPB_resvd_sec_cnt + n * bpb.BPB_FATsz32;
    }

    inline u32 getFirstDataSector(BPB &bpb) {
        return bpb.BPB_resvd_sec_cnt + bpb.BPB_num_fats * bpb.BPB_FATsz32;
    }

    inline u32 getFirstSectorOfCluster(BPB &bpb, u32 n) {
        return (n - 2) * bpb.BPB_sec_per_clus + getFirstDataSector(bpb);
    }

    inline u32 getDataSecCnt(BPB &bpb) {
        u32 root_dir_sectors = 0; // it's always zero on fat32
        return bpb.BPB_tot_sec32 - (bpb.BPB_resvd_sec_cnt + bpb.BPB_num_fats * bpb.BPB_FATsz32 + root_dir_sectors);
    }

    inline u32 bytesPerClus(BPB &bpb) {
        return bpb.BPB_sec_per_clus * bpb.BPB_bytes_per_sec;
    }

    /**
     * Note that the CountOfClusters value is the count of data clusters starting at cluster 2,
     * so the maximum valid cluster number for the volume is CountOfClusters + 1, and the "count of clusters including
     * the two reserved clusters" is CountOfClusters + 2(from the white paper).
     * */
    inline u32 getCountOfClusters(BPB &bpb) {
        return getDataSecCnt(bpb) / bpb.BPB_sec_per_clus;
    }

    struct FATPos {
        u32 fat_sec_num;
        u32 fat_ent_offset;
    };

    inline bool isValidCluster(u32 clus_no) {
        return clus_no != 0 && clus_no != 1;
    }

    FATPos getClusPosOnFAT(BPB &bpb, u32 n);

    inline u32 readFATClusEntryVal(const u8 *sec_buff, u32 fat_ent_offset) {
        return (*((u32 *) &sec_buff[fat_ent_offset])) & 0x0FFFFFFF;
    }

    void writeFATClusEntryVal(u8 *sec_buff, u32 fat_ent_offset, u32 fat_clus_entry_val);

    /**
     * EOC checker function
     * */
    inline bool isEndOfClusChain(u32 clus_no) {
        return clus_no >= 0x0FFFFFF8;
    }

    // todo
    u8 calcSecPerClus(u64 disk_sz);

    // todo
    u32 calcFatsz(BPB &bpb);

    struct FSInfo {
        u32 lead_sig;
        byte reserved1[480];
        u32 struc_sig;
        u32 free_count;
        u32 nxt_free;
        byte reserved2[12];
        u32 trail_sig;
    } __attribute__((packed));

    void assertFSInfo(FSInfo &fs_info);

    // todo
    FSInfo makeFSInfo();

    typedef u8 TimeTenth;
    typedef u16 FatTime;
    typedef u16 FatDate;
    struct FatTimeStamp {
        FatTime time;
        FatDate date;
    }__attribute__((packed));

    struct FatTimeStamp2 {
        TimeTenth tt;
        FatTimeStamp ts;
    }__attribute__((packed));

    FatTimeStamp unix2DosTs(timespec unix_ts);

    timespec dos2UnixTs(FatTimeStamp fat_ts);

    FatTimeStamp2 unix2DosTs_2(timespec unix_ts);

    timespec dos2UnixTs_2(FatTimeStamp2 fat_ts2);

    fat32::FatTimeStamp getCurDosTs() noexcept;

    fat32::FatTimeStamp2 getCurDosTs2() noexcept;

    struct ShortDirEntry {
        u8 name[11];
        byte attr;
        byte rsvd;
        FatTimeStamp2 crt_ts2;
        FatDate lst_acc_date;
        u16 fst_clus_high;
        FatTimeStamp wrt_ts;
        u16 fst_clus_low;
        u32 file_sz;
    }__attribute__((packed));

    enum EntryType {
        KEmpty, KEnd, KExist
    };

    EntryType parseEntryType(ShortDirEntry &short_dir_entry);

    util::string_gbk readShortEntryName(ShortDirEntry &short_dir_entry);

    // todo: fix this
    bool containIllegalShortDirEntryChr(u8 val);

    inline void setFstClusNo(ShortDirEntry &short_dir_entry, u32 clus) {
        short_dir_entry.fst_clus_high = (u16) (clus >> 16);
        short_dir_entry.fst_clus_low = (u16) (clus & 0xffff);
    }

    inline u32 readEntryClusNo(const ShortDirEntry &short_dir_entry) {
        return (short_dir_entry.fst_clus_high << 16) | short_dir_entry.fst_clus_low;
    }

    inline bool isDirectory(ShortDirEntry &short_dir_entry) {
        return short_dir_entry.attr & KAttrDirectory;
    }

    struct LongDirEntry {
        u8 ord;
        u8 name1[10];
        u8 attr;
        u8 type;
        u8 chk_sum;
        u8 name2[12];
        u16 fst_clus_low;
        u8 name3[4];
    }__attribute__((packed));

    util::string_utf16 readLongEntryName(const LongDirEntry &long_dir_entry);

    inline bool isLongDirEntry(const LongDirEntry &dir_entry) {
        return (dir_entry.attr & KAttrLongNameMask) == KAttrLongName;
    }

    inline bool isValidLongDirEntry(const LongDirEntry &dir_entry, u8 chk_sum) {
        return isLongDirEntry(dir_entry)
               && dir_entry.type == 0
               && dir_entry.chk_sum == chk_sum
               && dir_entry.fst_clus_low == 0;
    }

    inline ShortDirEntry &castLongDirEntryToShort(LongDirEntry &l_dir_entry) {
        return *(ShortDirEntry *) &l_dir_entry;
    }

    inline LongDirEntry &castShortDirEntryToLong(ShortDirEntry &s_dir_entry) {
        return *(LongDirEntry *) &s_dir_entry;
    }

    LongDirEntry mkLongDirEntry(bool is_lst, u8 ord, u8 chk_sum, util::string_utf16 &name, u32 off);

    inline bool isEmptyDirEntry(LongDirEntry &dir_entry) {
        return dir_entry.ord == 0x00 || dir_entry.ord == 0xE5;
    }

    inline bool isLstEmptyDirEntry(LongDirEntry &dir_entry) {
        return dir_entry.ord == 0x00;
    }

    inline void setDirEntryEmpty(LongDirEntry &dir_entry) {
        dir_entry.ord = 0xE5;
    }

    inline void setDirEntryLstEmpty(LongDirEntry &dir_entry) {
        dir_entry.ord = 0x00;
    }

    // Assume utf8_str can always be converted to a gbk encoding.
    struct BasisName {
        char primary[8 + 1];
        char extension[3 + 1];
    }__attribute__((packed));

    ShortDirEntry mkShortDirEntry(const BasisName &basis_name, bool is_dir);

    ShortDirEntry mkDotShortDirEntry(fat32::FatTimeStamp2 fat_ts2, u32 fst_clus);

    ShortDirEntry mkDotDotShortDirEntry(fat32::FatTimeStamp2 fat_ts2, u32 fst_clus);

    u8 chkSum(BasisName &basis_name);

    BasisName mkEmptyBasisName();

    BasisName genBasisNameFromShort(util::string_gbk short_name);

    BasisName genBasisNameFromLong(util::string_utf8 long_name);

    class FAT {
    public:
        FAT(BPB bpb, u32 free_count, std::shared_ptr<device::Device> device) noexcept;

        u64 availClusCnt() noexcept;

        std::optional<std::vector<u32>> allocClus(u32 require_clus_num) noexcept;

        // fst_clus should only be a file's first cluster
        void freeClus(u32 fst_clus) noexcept;

        std::vector<u32> readClusChains(u32 fst_clus) noexcept;

        // If fst_clus is invalid, it may be written with a valid cluster number;
        // If clus_num is zero, fst_clus will be written with a zero;
        // In other situation, fst_clus won't be changed.
        //
        // If clear is set, the new allocated clusters will be set zero.
        bool resize(u32 &fst_clus, u32 clus_num, bool clear) noexcept;

        void clearClusChain(const std::vector<u32> &clus_chain) noexcept;

        // todo: perform writing on all the fats
        void writeFatEntry(u32 sec_no, u32 fat_ent_offset, u32 val) noexcept;

        // todo: perform reading on all the fats
        u32 readFatEntry(u32 sec_no, u32 fat_ent_no) noexcept;

    private:
        u32 start_sec_no_;
        u32 fat_sec_num_;
        u32 cnt_of_clus_;
        BPB bpb_;
        u32 free_count_;
        std::optional<u64> avail_clus_cnt_;
        std::shared_ptr<device::Device> device_;

        // todo: rename
        u32 end_sec_no() const { return this->start_sec_no_ + this->fat_sec_num_; }

        void inc_avail_cnt(u64 no) noexcept;

        void dec_avail_cnt(u64 no) noexcept;
    };

} // namespace fat32

#endif //STUPID_FAT32_FAT32_H
