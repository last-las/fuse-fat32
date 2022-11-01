#include <cassert>

#include "fat32.h"

namespace fat32 {
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
}