#ifndef STUPID_FAT32_FAT32_H
#define STUPID_FAT32_FAT32_H

#include "device.h"
#include "util.h"

namespace fat32 {
    struct BPB {
    };

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
