#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/vfs.h>
#include <ctime>
#include <fcntl.h>
#include <cstring>
#include <cstdio>

#include "gtest/gtest.h"

#include "common.h"

/**
 * The test cases for linux system calls under fuse-fat32. Some of them might fail if they are running on other filesystems.
 *
 * Google Test does not run tests in parallel, so don't worry about `errno` being overwritten by other threads.
 * */

char buffer[BUF_SIZE];
char buffer2[BUF_SIZE];
const char test_dir[] = "test_dir";
const char non_exist_file[] = "non_exist_file";
const char simple_file1[] = "short.txt";
const char long_name_file[] = "longest_name_in_the_world.txt";
const char non_empty_file[] = "content.txt";
const char tmp_file[] = "tmp.txt"; // dynamic used and deleted by test cases
const char non_empty_dir_file1[] = "non_empty_dir/sub_file1.txt";
const char non_empty_dir_file2[] = "non_empty_dir/sub_file2.txt";
const char invalid_fat32_names[8][20] = {
        "abc\\", "abc:", "abc*", "abc?", "abc\"", "abc<", "abc>", "abc|"
};
const char non_empty_dir_dir[] = "non_empty_dir/sub_dir";
const char tmp_dir[] = "tmp";
const char non_exist_dir[] = "non_exist_dir";
const char non_empty_dir[] = "non_empty_dir";
const char short_name_dir[] = "short_dir";
// TODO: use char[][] instead.
std::vector<std::string> multiple_path{
        "sub_dir1",
        "sub_dir1/sub_dir2",
        "sub_dir1/sub_dir2/sub_dir3",
        "sub_dir1/sub_dir2/sub_dir3/sub_dir4"
};

class TestSysCallEnv : public testing::Environment {
public:
    ~TestSysCallEnv() override = default;


    void SetUp() override {
        crtDirOrExist(test_dir);
        chdir(test_dir);
        crtFileOrExist(simple_file1);
        crtFileOrExist(long_name_file);
        crtFileOrExist(non_empty_file);
        crtDirOrExist(short_name_dir);
        crtDirOrExist(non_empty_dir);
        crtFileOrExist(non_empty_dir_file1);
        crtFileOrExist(non_empty_dir_file2);
        crtDirOrExist(non_empty_dir_dir);
    }

    void TearDown() override {
        chdir("..");
        rmDirRecur(test_dir, true);
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
    ASSERT_EQ(mkdir(dir, 0755), 0) << strerror(errno);
}

void rm_file(const char *file) {
    ASSERT_EQ(unlink(file), 0) << strerror(errno);

}

void rm_dir(const char *dir) {
    ASSERT_EQ(rmdir(dir), 0) << strerror(errno);
}

TEST(OpenTest, NonExistFile) {
    assert_file_non_exist(non_exist_file);
}

TEST(OpenTest, ExistFile) {
    assert_file_exist(simple_file1);
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
    int fd = open(simple_file1, O_RDONLY);
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
    // before chmod
    int fd = open(simple_file1, O_RDONLY);
    ASSERT_GT(fd, 0);
    struct stat fstat_{};
    ASSERT_EQ(fstat(fd, &fstat_), 0);
    mode_t old_mode = fstat_.st_mode;

    // chmod
    fstat_.st_mode ^= 0001;
    ASSERT_EQ(chmod(simple_file1, fstat_.st_mode), 0);

    // after chmod
    ASSERT_EQ(fstat(fd, &fstat_), 0);
    ASSERT_EQ(fstat_.st_mode, old_mode);

    close(fd);
}

// This test only passes under fat32.
TEST(FstatTest, Chown) {
    int fd = open(simple_file1, O_RDONLY);
    ASSERT_GT(fd, 0);
    struct stat fstat_{};
    ASSERT_EQ(fstat(fd, &fstat_), 0);

    errno = 0;
    ASSERT_EQ(chown(simple_file1, 1, 1), -1);
    ASSERT_EQ(errno, EPERM);

    close(fd);
}

TEST(MkdirTest, CrtExistDir) {
    errno = 0;
    ASSERT_EQ(mkdir(short_name_dir, 0755), -1);
    ASSERT_EQ(errno, EEXIST);

    errno = 0;
    ASSERT_EQ(mkdir(simple_file1, 0755), -1);
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

// This test case only passes under fat32
TEST(MkdirTest, ConstantMode) {
    ASSERT_EQ(mkdir(tmp_dir, 0741), 0);
    int fd = open(tmp_dir, O_DIRECTORY);
    struct stat fstat_{};
    ASSERT_EQ(fstat(fd, &fstat_), 0);
    EXPECT_EQ(fstat_.st_mode & 0777, 0755);

    // close
    EXPECT_EQ(close(fd), 0);

    // recover
    rm_dir(tmp_dir);
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


TEST(RWTest, LessThanOneClus) {
    // prepare a buffer
    int sz = SEC_SIZE / 2;
    char byte = 0x34;
    memset(buffer, byte, sz);

    // write to file
    int fd = open(tmp_file, O_WRONLY | O_CREAT, 0644);
    ASSERT_GT(fd, 0);
    ssize_t cnt = write(fd, buffer, sz);
    ASSERT_EQ(cnt, sz);
    ASSERT_EQ(close(fd), 0);

    // clear buffer first
    memset(buffer, 0, sz);

    // read from file
    fd = open(tmp_file, O_RDONLY);
    ASSERT_GT(fd, 0);
    cnt = read(fd, buffer, sz);
    ASSERT_EQ(cnt, sz);
    for (int i = 0; i < cnt; ++i) {
        ASSERT_EQ(buffer[i], byte);
    }
    ASSERT_EQ(close(fd), 0);

    // recover
    rm_file(tmp_file);
}

TEST(RWTest, MoreThanOneClus) {
    // prepare a buffer
    int sz = BUF_SIZE;
    char byte = 0x34;
    memset(buffer, byte, sz);

    // write to file
    int fd = open(tmp_file, O_WRONLY | O_CREAT, 0644);
    ASSERT_GT(fd, 0);
    ssize_t cnt = write(fd, buffer, sz);
    ASSERT_EQ(cnt, sz);
    ASSERT_EQ(close(fd), 0);

    // clear buffer first
    memset(buffer, 0, sz);

    // read from file
    fd = open(tmp_file, O_RDONLY);
    ASSERT_GT(fd, 0);
    cnt = read(fd, buffer, sz);
    ASSERT_EQ(cnt, sz);
    for (int i = 0; i < cnt; ++i) {
        ASSERT_EQ(buffer[i], byte);
    }
    ASSERT_EQ(close(fd), 0);

    // recover
    rm_file(tmp_file);
}

TEST(RWTest, MoreThanFileSz) {
    // prepare a buffer
    int sz = SEC_SIZE / 2;
    char byte = 0x34;
    memset(buffer, byte, sz);

    // write to file
    int fd = open(tmp_file, O_WRONLY | O_CREAT, 0644);
    ASSERT_GT(fd, 0);
    ssize_t cnt = write(fd, buffer, sz);
    ASSERT_EQ(cnt, sz);
    ASSERT_EQ(close(fd), 0);

    // read from file
    int big_sz = sz * 3;
    fd = open(tmp_file, O_RDONLY);
    ASSERT_GT(fd, 0);
    cnt = read(fd, buffer, big_sz);
    ASSERT_EQ(cnt, sz);

    // recover
    rm_file(tmp_file);
}

TEST(RWTest, MultipleTimes) {
    // prepare a buffer
    int sz = SEC_SIZE / 2;
    char byte = 0x34;

    // write to file
    int fd = open(tmp_file, O_WRONLY | O_CREAT, 0644);
    ASSERT_GT(fd, 0);
    ssize_t cnt;
    char tmp_byte = byte;
    for (int i = 0; i < sz; ++i) {
        cnt = write(fd, &tmp_byte, 1);
        ASSERT_EQ(cnt, 1);
    }
    ASSERT_EQ(close(fd), 0);

    // read from file
    fd = open(tmp_file, O_RDONLY);
    ASSERT_GT(fd, 0);
    for (int i = 0; i < sz; ++i) {
        tmp_byte = 0;
        cnt = read(fd, &tmp_byte, 1);
        ASSERT_EQ(cnt, 1);
        ASSERT_EQ(tmp_byte, byte);
    }
    ASSERT_EQ(close(fd), 0);

    // recover
    rm_file(tmp_file);
}

TEST(ReadDirTest, ListDir) {
    int dir_fd = open(non_empty_dir, O_DIRECTORY);
    ASSERT_GT(dir_fd, 0);

    struct linux_dirent *d;
    int nread = syscall(SYS_getdents, dir_fd, buffer, BUF_SIZE);
    ASSERT_GT(nread, 0);
    std::vector<std::string> files;
    for (int bpos = 0; bpos < nread;) {
        d = (struct linux_dirent *) (buffer + bpos);
        files.emplace_back(d->d_name);
        bpos += d->d_reclen;
    }
    std::sort(files.begin(), files.end());
    ASSERT_STREQ(files[0].c_str(), ".");
    ASSERT_STREQ(files[1].c_str(), "..");
    ASSERT_STREQ(files[2].c_str(), "sub_dir");
    ASSERT_STREQ(files[3].c_str(), "sub_file1.txt");
    ASSERT_STREQ(files[4].c_str(), "sub_file2.txt");

    ASSERT_EQ(close(dir_fd), 0);
}

// This test case only passes under fat32
TEST(StatfsTest, Statfs) {
    struct statfs stat{};
    ASSERT_EQ(statfs("./", &stat), 0);
    ASSERT_EQ(stat.f_type, 0x4d44); // MSDOS_SUPER_MAGIC
    ASSERT_GT(stat.f_bsize, 0);
    ASSERT_GT(stat.f_blocks, 0);
    ASSERT_GT(stat.f_bfree, 0);
    ASSERT_GT(stat.f_bavail, 0);
    ASSERT_EQ(stat.f_files, 0);
    ASSERT_EQ(stat.f_ffree, 0);
    // TODO: ASSERT_GT(stat.f_namelen, 0);
}

// This test case only passes under fat32
TEST(CrtFileTest, ConstantMode) {
    int fd = creat(tmp_file, S_IFREG);
    EXPECT_GT(fd, 0);
    struct stat stat_{};
    EXPECT_EQ(fstat(fd, &stat_), 0);
    EXPECT_EQ(stat_.st_mode & 0777, 0755);

    EXPECT_EQ(close(fd), 0);
    rm_file(tmp_file);
}

// This test case only passes under fat32
TEST(CrtFileTest, IllegalFatChr) {
    std::vector<int> fd_vec;
    for (int i = 0; i < 8; ++i) {
        errno = 0;
        int fd = creat(invalid_fat32_names[i], S_IFREG);
        fd_vec.push_back(fd);
        EXPECT_EQ(fd, -1);
        EXPECT_EQ(errno, EINVAL);
    }

    // recover
    if (fd_vec[0] > 0) {
        for (const auto &fd: fd_vec) {
            EXPECT_EQ(close(fd), 0);
        }

        for (int i = 0; i < 8; ++i) {
            rm_file(invalid_fat32_names[i]);
        }
    }
}

// This test case only passes under fat32
// require privilege to run `mknod`, otherwise this case will always be passed
TEST(MknodTest, SpecialFile) {
    errno = 0;
    int ret = mknod(tmp_file, S_IFCHR, 12345);
    EXPECT_EQ(ret, -1);
    EXPECT_EQ(errno, EPERM);

    if (ret == 0) {
        rm_file(tmp_file);
    }
}

// This test case only passes under fat32
TEST(LinkTest, HardLink) {
    errno = 0;
    int ret = link(simple_file1, non_exist_file);
    EXPECT_EQ(ret, -1);
    EXPECT_EQ(errno, EPERM);

    if (ret == 0) {
        rm_file(non_exist_file);
    }
}

// This test case only passes under fat32
TEST(LinkTest, SymLink) {
    errno = 0;
    int ret = symlink(simple_file1, non_exist_file);
    EXPECT_EQ(ret, -1);
    EXPECT_EQ(errno, EPERM);

    if (ret == 0) {
        rm_file(non_exist_file);
    }
}

// This test case only passes under fat32
TEST(RenameTest, IgnoreFlags) {
    errno = 0;
    int ret = renameat2(AT_FDCWD, simple_file1, AT_FDCWD, long_name_file, RENAME_EXCHANGE);
    EXPECT_EQ(ret, -1);
    if (ret == -1) {
        ASSERT_EQ(errno, EINVAL);
    } else { // recover to avoid influence the following test.
        renameat2(AT_FDCWD, simple_file1, AT_FDCWD, long_name_file, RENAME_EXCHANGE);
    }
}

TEST(RenameTest, SrcNonExist) {
    errno = 0;
    ASSERT_EQ(rename(non_exist_file, simple_file1), -1);
    ASSERT_EQ(errno, ENOENT);
}

TEST(RenameTest, SrcFileDstNonExist) {
    assert_file_exist(simple_file1);
    assert_file_non_exist(non_exist_file);
    ASSERT_EQ(rename(simple_file1, non_exist_file), 0);
    assert_file_non_exist(simple_file1);
    assert_file_exist(non_exist_file);

    // recover
    ASSERT_EQ(rename(non_exist_file, simple_file1), 0);
    assert_file_non_exist(non_exist_file);
    assert_file_exist(simple_file1);
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

    assert_file_exist(simple_file1);
    assert_file_exist(tmp_file);
    ASSERT_EQ(rename(simple_file1, tmp_file), 0);
    assert_file_non_exist(simple_file1);
    assert_file_exist(tmp_file);

    // recover
    ASSERT_EQ(rename(tmp_file, simple_file1), 0);
    assert_file_non_exist(tmp_file);
    assert_file_exist(simple_file1);
}

TEST(RenameTest, SrcFileDstDir) {
    create_file(tmp_file);
    create_dir(tmp_dir);
    errno = 0;
    ASSERT_EQ(rename(tmp_file, tmp_dir), -1);
    ASSERT_EQ(errno, EISDIR);
    rm_file(tmp_file);
    rm_dir(tmp_dir);
}

TEST(RenameTest, SrcDirDstFile) {
    create_dir(tmp_dir);
    create_file(tmp_file);
    errno = 0;
    ASSERT_EQ(rename(tmp_dir, tmp_file), -1);
    ASSERT_EQ(errno, ENOTDIR);
    rm_dir(tmp_dir);
    rm_file(tmp_file);
}

TEST(RenameTest, SrcDirDstDirEmpty) {
    create_dir(tmp_dir);
    assert_dir_non_exist(non_exist_dir);
    ASSERT_EQ(rename(tmp_dir, non_exist_dir), 0);
    assert_dir_non_exist(tmp_dir);
    assert_dir_exist(non_exist_dir);

    // recover
    rm_dir(non_exist_dir);
}

TEST(RenameTest, SrcDirDstDirNonEmpty) {
    errno = 0;
    ASSERT_EQ(rename(short_name_dir, non_empty_dir), -1);
    ASSERT_EQ(errno, ENOTEMPTY);
}

TEST(RenameTest, SubDir) {
    ASSERT_EQ(rename(non_empty_dir_file1, tmp_file), 0);

    // recover
    ASSERT_EQ(rename(tmp_file, non_empty_dir_file1), 0);
    assert_file_non_exist(tmp_file);
}

TEST(RenameTest, SameContent) {
    int fd1 = open(non_empty_file, O_RDONLY);
    ASSERT_GT(fd1, 0);
    size_t cnt1 = read(fd1, buffer, BUF_SIZE);
    ASSERT_EQ(rename(non_empty_file, tmp_file), 0);
    int fd2 = open(tmp_file, O_RDONLY);
    ASSERT_GT(fd2, 0);
    size_t cnt2 = read(fd2, buffer, BUF_SIZE);
    ASSERT_EQ(cnt1, cnt2);
    for (size_t i = 0; i < cnt1; ++i) {
        ASSERT_EQ(buffer[i], buffer2[i]);
    }

    // recover
    ASSERT_EQ(rename(tmp_file, non_empty_file), 0);
    assert_file_non_exist(tmp_file);
    assert_file_exist(non_empty_file);
}

int main(int argc, char **argv) {
    testing::AddGlobalTestEnvironment(new TestSysCallEnv);
    testing::InitGoogleTest(&argc, argv);
    int result = RUN_ALL_TESTS();
    printf("Warning: There are 10 test cases that are not suppose to pass under other filesystems.\n");
    printf("Warning: Some test cases require privilege mode to pass.\n");
    return result;
}

