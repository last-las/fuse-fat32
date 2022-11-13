#include "gtest/gtest.h"

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
