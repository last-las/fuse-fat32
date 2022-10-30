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

    const u32 fat32_eoc_mark = 0x0FFFFFF8;
    const u32 cln_shut_bit_mask = 0x08000000;
    const u32 hrd_err_bit_mask = 0x04000000;

    void assertFat32BPB(BPB &bpb) {
        // todo: check CountOfClusters >= 65525
    }

    BPB makeFat32BPB() {
        // todo: make sure that CountOfClusters >= 65525
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

    FATPos getClusPosOnFAT(BPB &bpb, u32 n) {
        u32 fat_offset = n * 4;
        u32 this_fat_sec_num = bpb.BPB_resvd_sec_cnt + fat_offset / bpb.BPB_bytes_per_sec;
        u32 this_fat_ent_offset = fat_offset % bpb.BPB_bytes_per_sec;
        FATPos pos{this_fat_sec_num, this_fat_ent_offset};
    }

    inline u32 readFATClusEntryVal(const u8 *sec_buff, u32 fat_ent_offset) {
        return (*((u32 *) &sec_buff[fat_ent_offset])) & 0x0FFFFFFF;
    }

    void writeFATClusEntryVal(u8 *sec_buff, u32 fat_ent_offset, u32 fat_clus_entry_val) {
        fat_clus_entry_val = fat_clus_entry_val & 0x0FFFFFFF;
        *((u32 *) &sec_buff[fat_ent_offset]) = (*((u32 *) &sec_buff[fat_ent_offset])) & 0xF0000000;
        *((u32 *) &sec_buff[fat_ent_offset]) = (*((u32 *) &sec_buff[fat_ent_offset])) | fat_clus_entry_val;
    }

    inline bool isEndOfClusChain(u32 fat_content) {
        return fat_content >= 0x0FFFFFF8;
    }


    struct FSInfo {
    };


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
    };


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
