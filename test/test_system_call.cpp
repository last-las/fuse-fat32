#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <ctime>
#include <fcntl.h>
#include <cstdio>

#include "gtest/gtest.h"

/**
 * The test cases for linux system calls under fat32. Some of them might fail if they are running on other filesystems.
 *
 * Google Test does not run tests in parallel, so don't worry about `errno` being overwritten by other threads.
 * */

const char non_exist_file[] = "non_exist_file";
const char short_name_file[] = "short.txt";
const char long_name_file[] = "longest_name_in_the_world.txt";
const char non_exist_dir[] = "non_exist_dir";
const char short_name_dir[] = "short_dir";
std::vector<std::string> multiple_path{
        "sub_dir1",
        "sub_dir1/sub_dir2",
        "sub_dir1/sub_dir2/sub_dir3",
        "sub_dir1/sub_dir2/sub_dir3/sub_dir4"
};

class TestSysCallEnv : public testing::Environment {
public:
    ~TestSysCallEnv() override = default;

    static void crtFileOrExist(const char *filename) {
        int fd = open(filename, O_CREAT, 0644);
        if (fd == -1 && errno != 0) {
            printf("Fail to create file %s: %s\n", filename, strerror(errno));
            exit(1);
        }
        close(fd);
    }

    static void crtDirOrExist(const char *dirname) {
        int ret = mkdir(dirname, 0755);
        if (ret == -1 && errno != 0) {
            printf("Fail to create dir %s: %s\n", dirname, strerror(errno));
            exit(1);
        }
    }

    // Remove as more files as possible, so do not exit when unlink fails.
    static void rmFile(const char *filename) {
        int ret = unlink(filename);
        if (ret == -1 && errno != 0) {
            printf("Fail to remove file %s: %s\n", filename, strerror(errno));
        }
    }

    // Remove as more directories as possible, so do not exit when unlink fails.
    static void rmDir(const char *dirname) {
        int ret = rmdir(dirname);
        if (ret == -1 && errno != 0) {
            printf("Fail to remove dir %s: %s\n", dirname, strerror(errno));
        }
    }

    void SetUp() override {
        crtFileOrExist(short_name_file);
        crtFileOrExist(long_name_file);
        crtDirOrExist(short_name_dir);
    }

    void TearDown() override {
        rmFile(short_name_file);
        rmFile(long_name_file);
        rmDir(short_name_dir);
    }
};


TEST(OpenTest, NonExistFile) {
    errno = 0;
    int fd = open(non_exist_file, O_RDONLY);
    ASSERT_EQ(fd, -1);
    ASSERT_EQ(errno, ENOENT);
}

TEST(OpenTest, ExistFile) {
    int fd = open(short_name_file, O_RDONLY);
    ASSERT_GT(fd, 0);
    ASSERT_EQ(close(fd), 0);
}

TEST(OpenTest, ExistDir) {
    int fd = open(short_name_dir, O_DIRECTORY);
    ASSERT_GT(fd, 0);
    ASSERT_EQ(close(fd), 0);
}

TEST(OpenTest, LongName) {
    int fd = open(long_name_file, O_RDONLY);
    ASSERT_GT(fd, 0);
    ASSERT_EQ(close(fd), 0);
}

bool operator!=(const timespec lhs, const timespec rhs) {
    return lhs.tv_sec != rhs.tv_sec || lhs.tv_nsec != rhs.tv_nsec;
}

bool operator==(const timespec lhs, const timespec rhs) {
    return lhs.tv_sec == rhs.tv_sec && lhs.tv_nsec == rhs.tv_nsec;
}

// we might need to sleep for 0.2ms under fat32..
TEST(FstatTest, Time) {
    int fd = open(short_name_file, O_RDONLY);
    struct stat fstat_{};
    ASSERT_EQ(fstat(fd, &fstat_), 0);

    struct timespec cur_ts{};
    ASSERT_EQ(clock_gettime(CLOCK_REALTIME, &cur_ts), 0);
    ASSERT_NE(cur_ts, fstat_.st_ctim);
    ASSERT_NE(cur_ts, fstat_.st_atim);
    ASSERT_NE(cur_ts, fstat_.st_mtim);

    ASSERT_EQ(close(fd), 0);
}

// This test only passes under fat32.
TEST(FstatTest, Chmod) {
    // TODO
}

// This test only passes under fat32.
TEST(FstatTest, Chown) {
    // TODO
}

TEST(MkdirTest, CrtExistDir) {
    errno = 0;
    ASSERT_EQ(mkdir(short_name_dir, 0755), -1);
    ASSERT_EQ(errno, EEXIST);

    errno = 0;
    ASSERT_EQ(mkdir(short_name_file, 0755), -1);
    ASSERT_EQ(errno, EEXIST);
}

TEST(MkdirTest, CrtMultipleLevelDir) {
    std::vector<int> fd_list;

    // create multiple dir
    for (const auto &path: multiple_path) {
        ASSERT_EQ(mkdirat(AT_FDCWD, path.c_str(), 0755), 0);
        int dir_fd = openat(AT_FDCWD, path.c_str(), O_DIRECTORY);
        ASSERT_GT(dir_fd, 0);
        fd_list.push_back(dir_fd);
    }

    // rm
    for (auto rit = multiple_path.rbegin(); rit != multiple_path.rend(); ++rit) {
        ASSERT_EQ(rmdir(rit->c_str()), 0);
    }

    // close
    for (const auto &dir_fd: fd_list) {
        ASSERT_EQ(close(dir_fd), 0);
    }
}

// This test will be passed only under fat32.
TEST(MkdirTest, ConstantMode) {
}

TEST(UnlinkTest, NonExist) {
    errno = 0;
    ASSERT_EQ(unlink(non_exist_file), -1);
    ASSERT_EQ(errno, ENOENT);
}

TEST(UnlinkTest, RmFile) {
    int fd = open(non_exist_file, O_CREAT, 0644);
    ASSERT_GT(fd, 0);
    ASSERT_EQ(close(fd), 0);
    ASSERT_EQ(unlink(non_exist_file), 0);
}

TEST(UnlinkTest, RmDir) {
    ASSERT_EQ(mkdir(non_exist_dir, 0755), 0);
    int dir_fd = open(non_exist_dir, O_DIRECTORY);
    ASSERT_GT(dir_fd, 0);
    ASSERT_EQ(close(dir_fd), 0);
    ASSERT_EQ(unlink(non_exist_dir), -1);
    ASSERT_EQ(rmdir(non_exist_dir), 0);
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

TEST(RWTest, LessThanOneClus) {
}

TEST(RWTest, MoreThanOneClus) {
}

TEST(RWTest, MoreThanFileSz) {
}

TEST(ReadDirTest, ListDir) {
}

TEST(StatfsTest, Statfs) {
}

TEST(CrtFileTest, SpecialFile) {
}

TEST(CrtFileTest, RegularFile) {
}

TEST(CrtFileTest, ConstantMode) {
}

TEST(CrtFileTest, IllegalFatChr) {
}

// todo: crt link file should be failed.

int main(int argc, char **argv) {
    testing::AddGlobalTestEnvironment(new TestSysCallEnv);
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

