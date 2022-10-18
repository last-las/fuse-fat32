#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <ctime>
#include <fcntl.h>
#include <cstdio>

#include "gtest/gtest.h"

/**
 * The test cases for linux system calls under fuse-fat32. Some of them might fail if they are running on other filesystems.
 *
 * Google Test does not run tests in parallel, so don't worry about `errno` being overwritten by other threads.
 * */

const char non_exist_file[] = "non_exist_file";
const char short_name_file[] = "short.txt";
const char non_empty_file[] = "content.txt";
const char tmp_file[] = "tmp.txt"; // dynamic used and deleted by test cases
const char tmp_dir[] = "tmp";
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
        crtFileOrExist(non_empty_file);
        crtDirOrExist(short_name_dir);
    }

    void TearDown() override {
        rmFile(short_name_file);
        rmFile(long_name_file);
        rmFile(non_empty_file);
        rmDir(short_name_dir);
    }
};

void assert_file_exist(const char *file) {
    int fd = open(file, O_RDONLY);
    ASSERT_GT(fd, 0);
    ASSERT_EQ(close(fd), 0);
}

void assert_dir_exist(const char *dir) {
    int dir_fd = open(dir, O_DIRECTORY);
    ASSERT_GT(dir_fd, 0);
    ASSERT_EQ(close(dir_fd), 0);
}

void assert_file_non_exist(const char *file) {
    errno = 0;
    int fd = open(file, O_RDONLY);
    ASSERT_EQ(fd, -1);
    ASSERT_EQ(errno, ENOENT);
}

void assert_dir_non_exist(const char *dir) {
    errno = 0;
    int fd = open(dir, O_DIRECTORY);
    ASSERT_EQ(fd, -1);
    ASSERT_EQ(errno, ENOENT);
}

int create_file_ret_fd(const char *file) {
    int fd = open(file, O_CREAT, 0644);
    EXPECT_GT(fd, 0);
    return fd;
}

void create_file(const char *file) {
    int fd = create_file_ret_fd(file);
    ASSERT_EQ(close(fd), 0);
}

void create_dir(const char *dir) {
    ASSERT_EQ(mkdir(dir, 0755), 0);
}

void rm_file(const char *file) {
    ASSERT_EQ(unlink(file), 0);

}

void rm_dir(const char *dir) {
    ASSERT_EQ(rmdir(dir), 0);
}

TEST(OpenTest, NonExistFile) {
    assert_file_non_exist(non_exist_file);
}

TEST(OpenTest, ExistFile) {
    assert_file_exist(short_name_file);
}

TEST(OpenTest, ExistDir) {
    assert_dir_exist(short_name_dir);
}

TEST(OpenTest, LongName) {
    assert_file_exist(long_name_file);
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

// This test will be passed only under fuse-fat32
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
    assert_dir_exist(non_exist_dir);
    ASSERT_EQ(unlink(non_exist_dir), -1);
    ASSERT_EQ(rmdir(non_exist_dir), 0);
}

// This test will be passed only under fuse-fat32
TEST(RenameTest, IgnoreFlags) {
    errno = 0;
    int ret = renameat2(AT_FDCWD, short_name_file, AT_FDCWD, long_name_file, RENAME_EXCHANGE);
    EXPECT_EQ(ret, -1);
    if (ret == -1) {
        ASSERT_EQ(errno, EINVAL);
    } else { // recover to avoid influence the following test.
        renameat2(AT_FDCWD, short_name_file, AT_FDCWD, long_name_file, RENAME_EXCHANGE);
    }
}

TEST(RenameTest, SrcNonExist) {
    errno = 0;
    ASSERT_EQ(rename(non_exist_file, short_name_file), -1);
    ASSERT_EQ(errno, ENOENT);
}

TEST(RenameTest, SrcFileDstNonExist) {
    assert_file_exist(short_name_file);
    assert_file_non_exist(non_exist_file);
    ASSERT_EQ(rename(short_name_file, non_exist_file), 0);
    assert_file_non_exist(short_name_file);
    assert_file_exist(non_exist_file);

    // recover
    ASSERT_EQ(rename(non_exist_file, short_name_file), 0);
    assert_file_non_exist(non_exist_file);
    assert_file_exist(short_name_file);
}

TEST(RenameTest, SrcDirDstNonExist) {
    assert_dir_exist(short_name_dir);
    assert_dir_non_exist(non_exist_dir);
    ASSERT_EQ(rename(short_name_dir, non_exist_dir), 0);
    assert_dir_non_exist(short_name_dir);
    assert_dir_exist(non_exist_dir);

    // recover
    ASSERT_EQ(rename(non_exist_dir, short_name_dir), 0);
    assert_dir_non_exist(non_exist_dir);
    assert_dir_exist(short_name_dir);
}

TEST(RenameTest, SrcFileDstFile) {
    create_file(tmp_file);

    assert_file_exist(short_name_file);
    assert_file_exist(tmp_file);
    ASSERT_EQ(rename(short_name_file, tmp_file), 0);
    assert_file_non_exist(short_name_file);
    assert_file_exist(tmp_file);

    // recover
    ASSERT_EQ(rename(tmp_file, short_name_file), 0);
    assert_file_non_exist(tmp_file);
    assert_file_exist(short_name_file);
}

TEST(RenameTest, SrcFileDstDir) {
    create_file(tmp_file);
    create_dir(tmp_dir);
    errno = -1;
    ASSERT_EQ(rename(tmp_file, tmp_dir), -1);
    ASSERT_EQ(errno, EISDIR);
    rm_file(tmp_file);
    rm_dir(tmp_dir);
}

TEST(RenameTest, SrcDirDstFile) {
}

TEST(RenameTest, SrcDirDstDirEmpty) {
}

TEST(RenameTest, SrcDirDstDirNonEmpty) {
}

TEST(RenameTest, SubDir) {
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

// todo:
//  crt link file should be failed;
//  create a tmp file, do all the test and return;
//  test renamed file content;
//  basic test cases should set in front of complicated ones, eg, rename.

int main(int argc, char **argv) {
    testing::AddGlobalTestEnvironment(new TestSysCallEnv);
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

