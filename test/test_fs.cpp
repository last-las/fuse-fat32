#include <fcntl.h>
#include <linux/loop.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <unistd.h>
#include <cstdlib>

#include "gtest/gtest.h"

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
std::optional<fs::FAT32fs> filesystem;

class TestFsEnv : public testing::Environment {
public:
    void SetUp() override {
        add_regular_file(regular_file, block_sz);
        ASSERT_TRUE(add_loop_file(loop_name, regular_file)) << "Add loop file failed, skip the following tests\n";
        crtDirOrExist(mnt_point);
        mkfs_fat_on(loop_name);
        ASSERT_EQ(mount(loop_name, mnt_point, "vfat", 0, nullptr), 0);
    }

    void TearDown() override {
        ASSERT_EQ(umount(mnt_point), 0);
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

TEST(DirectoryTest, ListDir) {
    GTEST_SKIP();
}


int main(int argc, char **argv) {
    testing::AddGlobalTestEnvironment(new TestFsEnv);
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
