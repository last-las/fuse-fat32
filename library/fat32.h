#ifndef STUPID_FAT32_FAT32_H
#define STUPID_FAT32_FAT32_H

#include "device.h"
#include "util.h"

/**
 * For simplicity, Fat8/16 is not supported here.
 * */

namespace fat32 {
    struct BPB {
        byte BS_jmp_boot[3];
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
        byte BPB_reserved[12];
        u8 BS_drv_num;
        u8 BS_reserved1;
        u8 BS_boot_sig;
        u32 BS_vol_id;
        char BS_vol_lab[11];
        char BS_fil_sys_type[8];
    } __attribute__((packed));

    const u32 KFat32EocMark = 0x0FFFFFF8;
    const u32 KClnShutBitMask = 0x08000000;
    const u32 KHrdErrBitMask = 0x04000000;
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
    const u8 KAttrLongName = KAttrReadOnly | KAttrHidden | KAttrSystem | KAttrVolumeID;
    const u8 KInvalidFatBytes[] = {0x22, 0x2A, 0x2B, 0x2C, 0x2E, 0x2F, 0x3A, 0x3B,
                                   0x3C, 0x3D, 0x3E, 0x3F, 0x5B, 0x5C, 0x5D, 0x7c};

    // todo: check CountOfClusters >= 65525
    void assertFat32BPB(BPB &bpb);

    // todo: make sure that CountOfClusters >= 65525
    BPB makeFat32BPB();

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
    typedef short FatTime;
    typedef short FatDate;
    struct FatTimeStamp {
        FatTime time;
        FatDate date;
    };

    struct FatTimeStamp2 {
        TimeTenth tt;
        FatTimeStamp ts;
    };

    struct ShortDirEntry {
        char name[11];
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

    bool containIllegalShortDirEntryChr(const char *name);

    struct LongDirEntry {
    };

    class FAT {
    public:
        u64 availClusCnt() noexcept {}

        u32 allocClus() noexcept {}

        bool freeClus(u32 fst_clus) noexcept {}

        std::list<u32> readClusChains(u32 fst_clus) noexcept {}

        bool resize(u32 fst_clus, u32 clus_num) noexcept {}

    private:
        u32 start_sec_;
        u32 fat_sz_;
        device::Device &device_;
    };


} // namespace fat32

#endif //STUPID_FAT32_FAT32_H
