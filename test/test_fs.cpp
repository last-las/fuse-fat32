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
        device::LinuxFileDriver linuxFileDriver(regular_file);
        device::CacheManager cacheManager(linuxFileDriver);
        filesystem = std::make_unique<fs::FAT32fs>(fs::FAT32fs(cacheManager));
    }

    void TearDown() override {
        ASSERT_EQ(umount(mnt_point), 0);
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
}

TEST(FAT32Test, IllegalChr) {
    ASSERT_TRUE(fat32::containIllegalFatChr("\\"));
    ASSERT_TRUE(fat32::containIllegalFatChr("/"));
    ASSERT_TRUE(fat32::containIllegalFatChr(":"));
    ASSERT_TRUE(fat32::containIllegalFatChr("*"));
    ASSERT_TRUE(fat32::containIllegalFatChr("?"));
    ASSERT_TRUE(fat32::containIllegalFatChr("\""));
    ASSERT_TRUE(fat32::containIllegalFatChr("<"));
    ASSERT_TRUE(fat32::containIllegalFatChr(">"));
    ASSERT_TRUE(fat32::containIllegalFatChr("|"));
    ASSERT_FALSE(fat32::containIllegalFatChr("\x05"));
    ASSERT_FALSE(fat32::containIllegalFatChr("..regular name"));
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
