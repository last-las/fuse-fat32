#ifndef STUPID_FAT32_FAT32_H
#define STUPID_FAT32_FAT32_H

#include <ctime>

#include "device.h"
#include "util.h"

/**
 * For simplicity, Fat8/16 is not supported here.
 * */

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

    FATPos getClusPosOnFAT(BPB &bpb, u32 n);

    inline u32 readFATClusEntryVal(const u8 *sec_buff, u32 fat_ent_offset) {
        return (*((u32 *) &sec_buff[fat_ent_offset])) & 0x0FFFFFFF;
    }

    void writeFATClusEntryVal(u8 *sec_buff, u32 fat_ent_offset, u32 fat_clus_entry_val);

    /**
     * EOC checker function
     * */
    inline bool isEndOfClusChain(u32 fat_content) {
        return fat_content >= 0x0FFFFFF8;
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
    };

    struct FatTimeStamp2 {
        TimeTenth tt;
        FatTimeStamp ts;
    };

    FatTimeStamp unix2DosTs(timespec unix_ts);

    timespec dos2UnixTs(FatTimeStamp fat_ts);

    FatTimeStamp2 unix2DosTs_2(timespec unix_ts);

    timespec dos2UnixTs_2(FatTimeStamp2 fat_ts2);

    struct ShortDirEntry {
        u8 name[11];
        byte attr;
        byte rsvd;
        u8 crt_time_tenth;
        FatTimeStamp crt_ts;
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

    std::string readShortEntryName(ShortDirEntry &short_dir_entry);

    // todo: fix this
    bool containIllegalShortDirEntryChr(const char *name);

    inline void setEntryClusNo(ShortDirEntry &short_dir_entry, u32 clus) {
        short_dir_entry.fst_clus_high = (u16) (clus >> 16);
        short_dir_entry.fst_clus_low = (u16) (clus & 0xffff);
    }

    inline u32 readEntryClusNo(ShortDirEntry &short_dir_entry) {
        return (short_dir_entry.fst_clus_high << 16) | short_dir_entry.fst_clus_low;
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

    u8 chkSum(const u8 *short_entry_name);

    util::string_gbk genShortNameFrom(util::string_utf8 &utf8_str);

    class FAT {
    public:
        FAT(BPB bpb, u32 free_count, device::Device &device) noexcept;

        u64 availClusCnt() noexcept;

        std::optional<std::vector<u32>> allocClus(u32 require_clus_num) noexcept;

        // fst_clus should only be a file's first cluster
        void freeClus(u32 fst_clus) noexcept;

        std::vector<u32> readClusChains(u32 fst_clus) noexcept;

        bool resize(u32 fst_clus, u32 clus_num) noexcept;

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
        device::Device &device_;

        u32 end_sec_no() const { return this->start_sec_no_ + this->fat_sec_num_; }

        void inc_avail_cnt(u64 no) noexcept;

        void dec_avail_cnt(u64 no) noexcept;
    };


} // namespace fat32

#endif //STUPID_FAT32_FAT32_H
