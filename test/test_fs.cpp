#include <fcntl.h>
#include <linux/loop.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <cstring>

#include "gtest/gtest.h"

#include "fs.h"
#include "common.h"

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
    }

    void TearDown() override {
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
