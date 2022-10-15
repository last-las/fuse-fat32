#include "gtest/gtest.h"

TEST(OpenTest, NonExistFile) {
}

TEST(OpenTest, ExistFile) {
}

TEST(OpenTest, ExistDir) {
}

TEST(OpenTest, LongName) {
}

TEST(OpenTest, MultipleDir) {
}

TEST(FstatTest, Fat32ConstantValue) {
}

TEST(FstatTest, ChangeTime) {
}

TEST(MkdirTest, CrtSimpleDir) {
}

TEST(MkdirTest, CrtExistDir) {
}

TEST(MkdirTest, CrtMultipleLevelDir) {
}

TEST(MkdirTest, ConstantMode) {
}

TEST(UnlinkTest, NonExist) {
}

TEST(UnlinkTest, RmFile) {
}

TEST(UnlinkTest, RmDir) {
}

TEST(RmdirTest, RmDir) {
}

TEST(RenameTest, IgnoreFlags) {
}

TEST(RenameTest, SrcNonExist) {
}

TEST(RenameTest, SrcFileDstNonExist) {
}

TEST(RenameTest, SrcDirDstNonExist) {
}

TEST(RenameTest, SrcFileDstFile) {
}

TEST(RenameTest, SrcFileDstDir) {
}

TEST(RenameTest, SrcDirDstFile) {
}

TEST(RenameTest, SrcDirDstDirEmpty) {
}

TEST(RenameTest, SrcDirDstDirNonEmpty) {
}


// TODO: remove all created test files.

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

