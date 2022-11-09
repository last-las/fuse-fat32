#include <fcntl.h>
#include <linux/loop.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/vfs.h>
#include <unistd.h>
#include <cstdlib>

#include "gtest/gtest.h"

#include "fat32.h"
#include "fs.h"
#include "common.h"

/**
 * The test case requires mkfs.fat to be installed.
 * */
// TODO: use `mkfs.fat` to create a fat32, add some files and dirs, then test fs structure on it.
u64 block_sz = 1024 * 1024 * 1024; // 1G
char regular_file[] = "regular_file";
char loop_name[512];
char mnt_point[] = "fat32_mnt";
std::unique_ptr<fs::FAT32fs> filesystem;

class Fat32Filesystem {
public:
    Fat32Filesystem() {
        // create and mount a fat32 filesystem
        add_regular_file(regular_file, block_sz);
        if (!add_loop_file(loop_name, regular_file)) {
            printf("Add loop file failed, skip the following tests\n");
            exit(1);
        }
        crtDirOrExist(mnt_point);
        mkfs_fat_on(loop_name);
        if (mount(loop_name, mnt_point, "vfat", 0, nullptr) != 0) {
            printf("Mount failed, skip the following tests\n");
            exit(1);
        }
    }

    ~Fat32Filesystem() {
        if (umount(mnt_point) != 0) {
            printf("errno:%d %s\n", errno, strerror(errno));
        }
        rm_loop_file(loop_name);
        rmDir(mnt_point);
        rmFile(regular_file);
    }

private:
    static void mkfs_fat_on(const char *device) {
        char command[64];
        sprintf(command, "mkfs.fat %s", device);
        int ret = system(command);
        ASSERT_EQ(ret, 0);
    }
};

class FATTest : public testing::Test, Fat32Filesystem {
public:
    FATTest() {
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

    bool isEmptyFAT() {
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

private:
    std::vector<u8> fat_snapshot;
    u32 start_sec_no_;
    u32 fat_sec_no_;
    u32 fat_num_;
};


TEST(FAT32Test, StructSz) {
    ASSERT_EQ(sizeof(fat32::BPB), 90);
    ASSERT_EQ(sizeof(fat32::FSInfo), 512);
    ASSERT_EQ(sizeof(fat32::ShortDirEntry), 32);
    ASSERT_EQ(sizeof(fat32::LongDirEntry), 32);
}

TEST(FAT32Test, IllegalChr) {
    ASSERT_TRUE(fat32::containIllegalShortDirEntryChr("\\"));
    ASSERT_TRUE(fat32::containIllegalShortDirEntryChr("/"));
    ASSERT_TRUE(fat32::containIllegalShortDirEntryChr(":"));
    ASSERT_TRUE(fat32::containIllegalShortDirEntryChr("*"));
    ASSERT_TRUE(fat32::containIllegalShortDirEntryChr("?"));
    ASSERT_TRUE(fat32::containIllegalShortDirEntryChr("\""));
    ASSERT_TRUE(fat32::containIllegalShortDirEntryChr("<"));
    ASSERT_TRUE(fat32::containIllegalShortDirEntryChr(">"));
    ASSERT_TRUE(fat32::containIllegalShortDirEntryChr("|"));
    ASSERT_TRUE(fat32::containIllegalShortDirEntryChr(".."));
    ASSERT_FALSE(fat32::containIllegalShortDirEntryChr("\x05"));
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

TEST_F(FATTest, AvailClusCnt) {
    device::LinuxFileDriver device(regular_file, SECTOR_SIZE);
    fat32::BPB bpb = *(fat32::BPB *) device.readSector(0).value()->read_ptr(0);
    fat32::FAT fat = fat32::FAT(bpb, 0xffffffff, device);

    struct statfs fs_stat{};
    ASSERT_EQ(statfs(mnt_point, &fs_stat), 0);

    u64 clus_cnt = fat.availClusCnt();
    ASSERT_EQ(clus_cnt * bpb.BPB_sec_per_clus * SECTOR_SIZE, fs_stat.f_bsize * fs_stat.f_bavail);
    // todo: alloc,free and then check.
}

TEST_F(FATTest, AllocFree) {
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
    ASSERT_TRUE(FATTest::isEmptyFAT());

    // large alloc
    clus_chain = fat.allocClus(avail_clus_cnt).value();
    fst_clus = clus_chain[0];
    fat.freeClus(fst_clus);
    ASSERT_TRUE(FATTest::isEmptyFAT());

    // random alloc
    ASSERT_EQ(fat.allocClus(100).value()[0], 3);
    ASSERT_EQ(fat.allocClus(100).value()[0], 103);
    ASSERT_EQ(fat.allocClus(100).value()[0], 203);
    fat.freeClus(103);
    ASSERT_EQ(fat.allocClus(50).value()[0], 103);

    fat.freeClus(3);
    fat.freeClus(103);
    fat.freeClus(203);
    ASSERT_TRUE(FATTest::isEmptyFAT());

    // failed alloc
    ASSERT_FALSE(fat.allocClus(avail_clus_cnt + 1).has_value());

    // check available cnt
    ASSERT_EQ(avail_clus_cnt, fat.availClusCnt());
}

TEST_F(FATTest, SetFreeCount) {
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
    ASSERT_TRUE(FATTest::isEmptyFAT());
}

TEST_F(FATTest, ReadClusChain) {
    device::LinuxFileDriver device(regular_file, SECTOR_SIZE);
    fat32::BPB bpb = *(fat32::BPB *) device.readSector(0).value()->read_ptr(0);
    fat32::FAT fat = fat32::FAT(bpb, 0xffffffff, device);
    std::vector<u32> clus_chain, read_clus_chain;

    clus_chain = fat.allocClus(5).value();
    read_clus_chain = fat.readClusChains(clus_chain[0]);
    ASSERT_EQ(clus_chain, read_clus_chain);

    // clear
    fat.freeClus(clus_chain[0]);
    ASSERT_TRUE(FATTest::isEmptyFAT());
}

TEST_F(FATTest, Resize) {
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
    ASSERT_TRUE(FATTest::isEmptyFAT());
}

TEST(FAT32fsTest, RootDir) {
    GTEST_SKIP();
    // todo: check root dir entries
}

TEST(FAT32fsTest, GetFileByIno) {
    GTEST_SKIP();
}

TEST(FAT32fsTest, OpenFileByIno) {
    GTEST_SKIP();
}

TEST(FileTest, BasicRW) {
    GTEST_SKIP();
}

TEST(FileTest, SyncWithMeta) {
    GTEST_SKIP();
}

TEST(FileTest, SyncWithoutMeta) {
    GTEST_SKIP();
}

TEST(FileTest, TruncateLess) {
    GTEST_SKIP();
}

TEST(FileTest, TruncateMore) {
    GTEST_SKIP();
}

TEST(FileTest, SetTime) {
    GTEST_SKIP();
}

// todo: check here.
TEST(FileTest, Rename) {
    GTEST_SKIP();
}

TEST(DirTest, crtFile) {
    GTEST_SKIP();
}

TEST(DirTest, crtDir) {
    GTEST_SKIP();
}

TEST(DirTest, DelFile) {
    GTEST_SKIP();
}

TEST(DirTest, ListDir) {
    GTEST_SKIP();
}

TEST(DirTest, Lookup) {
    GTEST_SKIP();
}

TEST(DirTest, Iterate) {
    GTEST_SKIP();
}

TEST(DirTest, JudgeEmpty) {
    GTEST_SKIP();
}


int main(int argc, char **argv) {
    // testing::AddGlobalTestEnvironment(new Fat32Filesystem);
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
