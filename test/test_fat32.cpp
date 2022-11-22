#include <fcntl.h>

#include <sys/vfs.h>
#include <cstdlib>

#include "gtest/gtest.h"

#include "fat32.h"
#include "device.h"
#include "common.h"

static std::vector<u8> fat_snapshot;
static u32 start_sec_no_;
static u32 fat_sec_no_;
static u32 fat_num_;

/**
 * The test case requires mkfs.fat to be installed.
 * */
class TestFat32Env : public testing::Environment, Fat32Filesystem {
public:
    TestFat32Env() {
        device::LinuxFileDriver device(regular_file, SECTOR_SIZE);
        fat32::BPB bpb = *(fat32::BPB *) device.readSector(0).value()->read_ptr(0);
        start_sec_no_ = fat32::getFirstFATSector(bpb, 0);
        fat_sec_no_ = bpb.BPB_FATsz32;
        fat_num_ = bpb.BPB_num_fats;

        // take snapshot of FAT
        fat_snapshot = std::vector<u8>(fat_sec_no_ * fat_num_ * SECTOR_SIZE);
        u8 *snapshot_ptr = &fat_snapshot[0];
        for (u32 sec_no = start_sec_no_; sec_no < start_sec_no_ + fat_sec_no_ * fat_num_; sec_no++) {
            auto sector = device.readSector(sec_no).value();
            memcpy(snapshot_ptr, sector->read_ptr(0), SECTOR_SIZE);
            snapshot_ptr += SECTOR_SIZE;
        }
    }
};

static bool isEmptyFAT() {
    device::LinuxFileDriver device(regular_file, SECTOR_SIZE);
    u8 const *snapshot_ptr = &fat_snapshot[0];
    for (u32 sec_no = start_sec_no_; sec_no < start_sec_no_ + fat_sec_no_ * fat_num_; sec_no++) {
        auto sector = device.readSector(sec_no).value();
        if (memcmp(snapshot_ptr, sector->read_ptr(0), SECTOR_SIZE) != 0) {
            return false;
        }
        snapshot_ptr += SECTOR_SIZE;
    }
    return true;
}


TEST(FAT32Test, StructSz) {
    ASSERT_EQ(sizeof(fat32::BPB), 90);
    ASSERT_EQ(sizeof(fat32::FSInfo), 512);
    ASSERT_EQ(sizeof(fat32::ShortDirEntry), 32);
    ASSERT_EQ(sizeof(fat32::LongDirEntry), 32);
}

TEST(FAT32Test, IllegalChr) {
    ASSERT_TRUE(fat32::containIllegalShortDirEntryChr('\\'));
    ASSERT_TRUE(fat32::containIllegalShortDirEntryChr('/'));
    ASSERT_TRUE(fat32::containIllegalShortDirEntryChr(':'));
    ASSERT_TRUE(fat32::containIllegalShortDirEntryChr('*'));
    ASSERT_TRUE(fat32::containIllegalShortDirEntryChr('?'));
    ASSERT_TRUE(fat32::containIllegalShortDirEntryChr('\"'));
    ASSERT_TRUE(fat32::containIllegalShortDirEntryChr('<'));
    ASSERT_TRUE(fat32::containIllegalShortDirEntryChr('>'));
    ASSERT_TRUE(fat32::containIllegalShortDirEntryChr('|'));
    ASSERT_TRUE(fat32::containIllegalShortDirEntryChr('.'));
    ASSERT_FALSE(fat32::containIllegalShortDirEntryChr('\x05'));
}

TEST(FAT32Test, EntryClusNo) {
    fat32::ShortDirEntry short_dir_entry{};
    u32 clus_no = 0xdeadbeef;
    setEntryClusNo(short_dir_entry, clus_no);
    ASSERT_EQ(short_dir_entry.fst_clus_high, 0xdead);
    ASSERT_EQ(short_dir_entry.fst_clus_low, 0xbeef);
    ASSERT_EQ(readEntryClusNo(short_dir_entry), clus_no);
}

TEST(FAT32Test, unixDosCvt) {
    timespec unix_ts;
    ASSERT_EQ(clock_gettime(CLOCK_REALTIME, &unix_ts), 0);
    fat32::FatTimeStamp dos_ts = fat32::unix2DosTs(unix_ts);
    timespec parsed_unix_ts = fat32::dos2UnixTs(dos_ts);
    timespec parsed_unix_ts2 = fat32::dos2UnixTs(fat32::unix2DosTs(parsed_unix_ts));

    time_t gap = unix_ts.tv_sec - parsed_unix_ts.tv_sec;
    ASSERT_TRUE(gap == 0 || gap == 1);
    ASSERT_EQ(parsed_unix_ts.tv_sec, parsed_unix_ts2.tv_sec);
}

TEST(FAT32Test, unixDosCvt2) {
    timespec unix_ts;
    ASSERT_EQ(clock_gettime(CLOCK_REALTIME, &unix_ts), 0);
    fat32::FatTimeStamp2 dos_ts = fat32::unix2DosTs_2(unix_ts);
    timespec parsed_unix_ts = fat32::dos2UnixTs_2(dos_ts);
    timespec parsed_unix_ts2 = fat32::dos2UnixTs_2(fat32::unix2DosTs_2(parsed_unix_ts));

    time_t sec_gap = unix_ts.tv_sec - parsed_unix_ts.tv_sec;
    ASSERT_TRUE(sec_gap == 0 || sec_gap == 1);
    ASSERT_EQ(unix_ts.tv_nsec / 10000000, parsed_unix_ts.tv_nsec / 10000000);
    ASSERT_EQ(parsed_unix_ts.tv_sec, parsed_unix_ts2.tv_sec);
}

TEST(FAT32Test, AvailClusCnt) {
    device::LinuxFileDriver device(regular_file, SECTOR_SIZE);
    fat32::BPB bpb = *(fat32::BPB *) device.readSector(0).value()->read_ptr(0);
    fat32::FAT fat = fat32::FAT(bpb, 0xffffffff, device);
    struct statfs fs_stat{};
    ASSERT_EQ(statfs(mnt_point, &fs_stat), 0);

    // check
    u64 clus_cnt = fat.availClusCnt() + 2; // linux fat driver seems to treat the two reserved clusters as available
    ASSERT_EQ(clus_cnt * bpb.BPB_sec_per_clus * SECTOR_SIZE, fs_stat.f_bsize * fs_stat.f_bfree);

    // alloc, free ant check again
    auto clus_chain = fat.allocClus(100).value();
    fat.freeClus(clus_chain[0]);
    clus_cnt = fat.availClusCnt() + 2;
    ASSERT_EQ(clus_cnt * bpb.BPB_sec_per_clus * SECTOR_SIZE, fs_stat.f_bsize * fs_stat.f_bfree);
}

TEST(FAT32Test, AllocFree) {
    device::LinuxFileDriver device(regular_file, SECTOR_SIZE);
    fat32::BPB bpb = *(fat32::BPB *) device.readSector(0).value()->read_ptr(0);
    fat32::FAT fat = fat32::FAT(bpb, 0xffffffff, device);
    std::vector<u32> clus_chain;
    u32 fst_clus;
    u64 avail_clus_cnt = fat.availClusCnt();

    // small alloc
    clus_chain = fat.allocClus(10).value();
    fst_clus = clus_chain[0];
    fat.freeClus(fst_clus);
    ASSERT_TRUE(isEmptyFAT());

    // large alloc
    clus_chain = fat.allocClus(avail_clus_cnt).value();
    fst_clus = clus_chain[0];
    fat.freeClus(fst_clus);
    ASSERT_TRUE(isEmptyFAT());

    // random alloc
    ASSERT_EQ(fat.allocClus(100).value()[0], 3);
    ASSERT_EQ(fat.allocClus(100).value()[0], 103);
    ASSERT_EQ(fat.allocClus(100).value()[0], 203);
    fat.freeClus(103);
    ASSERT_EQ(fat.allocClus(50).value()[0], 103);

    fat.freeClus(3);
    fat.freeClus(103);
    fat.freeClus(203);
    ASSERT_TRUE(isEmptyFAT());

    // failed alloc
    ASSERT_FALSE(fat.allocClus(avail_clus_cnt + 1).has_value());

    // check available cnt
    ASSERT_EQ(avail_clus_cnt, fat.availClusCnt());
}

TEST(FAT32Test, SetFreeCount) {
    u32 free_clus_no = 5;
    device::LinuxFileDriver device(regular_file, SECTOR_SIZE);
    fat32::BPB bpb = *(fat32::BPB *) device.readSector(0).value()->read_ptr(0);
    fat32::FAT fat = fat32::FAT(bpb, free_clus_no, device);

    auto clus_chain = fat.allocClus(5).value();
    ASSERT_EQ(clus_chain[0], 5);
    ASSERT_EQ(clus_chain[1], 3);
    ASSERT_EQ(clus_chain[2], 4);
    ASSERT_EQ(clus_chain[3], 6);
    ASSERT_EQ(clus_chain[4], 7);

    // clear
    fat.freeClus(clus_chain[0]);
    ASSERT_TRUE(isEmptyFAT());
}

TEST(FAT32Test, ReadClusChain) {
    device::LinuxFileDriver device(regular_file, SECTOR_SIZE);
    fat32::BPB bpb = *(fat32::BPB *) device.readSector(0).value()->read_ptr(0);
    fat32::FAT fat = fat32::FAT(bpb, 0xffffffff, device);
    std::vector<u32> clus_chain, read_clus_chain;

    clus_chain = fat.allocClus(5).value();
    read_clus_chain = fat.readClusChains(clus_chain[0]);
    ASSERT_EQ(clus_chain, read_clus_chain);

    // clear
    fat.freeClus(clus_chain[0]);
    ASSERT_TRUE(isEmptyFAT());
}

TEST(FAT32Test, Resize) {
    device::LinuxFileDriver device(regular_file, SECTOR_SIZE);
    fat32::BPB bpb = *(fat32::BPB *) device.readSector(0).value()->read_ptr(0);
    fat32::FAT fat = fat32::FAT(bpb, 0xffffffff, device);
    std::vector<u32> expect_result;

    u32 fst_clus = fat.allocClus(5).value()[0];
    // shrink
    ASSERT_TRUE(fat.resize(fst_clus, 2));
    expect_result = {3, 4};
    ASSERT_EQ(fat.readClusChains(fst_clus), expect_result);
    // grow
    ASSERT_TRUE(fat.resize(fst_clus, 7));
    expect_result = {3, 4, 5, 6, 7, 8, 9};
    ASSERT_EQ(fat.readClusChains(fst_clus), expect_result);

    // clear
    fat.freeClus(fst_clus);
    ASSERT_TRUE(isEmptyFAT());
}


int main(int argc, char **argv) {
    testing::AddGlobalTestEnvironment(new TestFat32Env);
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
