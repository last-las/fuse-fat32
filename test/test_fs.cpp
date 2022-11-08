#include <fcntl.h>
#include <linux/loop.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
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

class TestFsEnv : public testing::Environment {
public:
    void SetUp() override {
        // create and mount a fat32 filesystem
        add_regular_file(regular_file, block_sz);
        ASSERT_TRUE(add_loop_file(loop_name, regular_file)) << "Add loop file failed, skip the following tests\n";
        crtDirOrExist(mnt_point);
        mkfs_fat_on(loop_name);
        ASSERT_EQ(mount(loop_name, mnt_point, "vfat", 0, nullptr), 0);

        // todo: add multiple files and directories in this fat32 fs

        // create a global fs::FAT32fs object
        device::LinuxFileDriver linuxFileDriver(regular_file, SECTOR_SIZE);
        device::CacheManager cacheManager(linuxFileDriver);
        filesystem = std::make_unique<fs::FAT32fs>(fs::FAT32fs(cacheManager));
    }

    void TearDown() override {
        ASSERT_EQ(umount(mnt_point), 0) << strerror(errno);
        rm_loop_file(loop_name);
        rmDir(mnt_point);
        // rmFile(regular_file);
    }

private:
    static void mkfs_fat_on(const char *device) {
        char command[64];
        sprintf(command, "mkfs.fat %s", device);
        int ret = system(command);
        ASSERT_EQ(ret, 0);
    }
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

TEST(FAT32Test, FATAvailClusCnt) {
    // TODO: statfs; copy the fat table and test on that
    // GTEST_SKIP();
    device::LinuxFileDriver device(regular_file, SECTOR_SIZE);
    fat32::BPB bpb = *(fat32::BPB *) device.readSector(0).value()->read_ptr(0);
    u32 start_sec_no = fat32::getFirstFATSector(bpb, 0);
    fat32::FAT fat = fat32::FAT(bpb, 0xffffffff, device); // todo: fix here
    printf("start_sec_no: %u\n", start_sec_no);
    printf("calc: %lld\n", fat.availClusCnt());
    printf("%u\n", fat.allocClus().value());
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
    testing::AddGlobalTestEnvironment(new TestFsEnv);
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
