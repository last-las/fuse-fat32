#include "gtest/gtest.h"
#include "memory"
#include <sys/mount.h>

#include "common.h"
#include "fs.h"

std::unique_ptr<fs::FAT32fs> filesystem;
const char simple_file1[] = "short1.txt";
const char simple_file2[] = "short2.txt";
const char simple_file3[] = "short3.txt";
const char empty_file[] = "empty.txt";
const char simple_dir1[] = "simple1";
const char simple_dir2[] = "simple2";
const char long_name_file[] = "this_is_a_long_name_file.1234";

class TestFsEnv : public testing::Environment, Fat32Filesystem {
public:
    TestFsEnv() {
        auto linux_file_driver = std::make_shared<device::LinuxFileDriver>(util::getFullPath(regular_file),
                                                                           SECTOR_SIZE);
        auto cache_mgr = std::make_shared<device::CacheManager>(std::move(linux_file_driver));
        filesystem = fs::FAT32fs::from(cache_mgr);
        touchTestFiles();
    }

    ~TestFsEnv() override {
        recover();
    }

    static void touchTestFiles() {
        ASSERT_EQ(chdir(mnt_point), 0);
        crtFileOrExist(simple_file1);
        crtFileOrExist(simple_file2);
        crtFileOrExist(simple_file3);
        crtFileOrExist(empty_file);
        crtFileOrExist(long_name_file);
        crtDirOrExist(simple_dir1);
        crtDirOrExist(simple_dir2);
        sync();
    }

    static void recover() {
        ASSERT_EQ(chdir(".."), 0);
    }

    static void reMount() {
        chdir("..");

        if (wrappedUmount(mnt_point) != 0) {
            printf("umount failed, errno: %d %s\n", errno, strerror(errno));
            exit(1);
        }
        if (mount(loop_name, mnt_point, "vfat", 0, nullptr) != 0) {
            printf("Mount failed, skip the following tests\n");
            exit(1);
        }

        chdir(mnt_point);
    }
};

TEST(FAT32fsTest, RootDir) {
    {
        auto root = filesystem->getRootDir();
        ASSERT_EQ(root->ino(), fs::KRootDirIno);
    }
    filesystem->flush();
}

TEST(FAT32fsTest, GetFileByIno) {
    GTEST_SKIP();
}

TEST(FAT32fsTest, OpenFileByIno) {
    GTEST_SKIP();
}

TEST(FileTest, SimpleRead) {
    {
        char buff[100] = {0};
        std::string simple_string = "hello world!";
        auto wrt_sz = writeFile(simple_file1, simple_string.c_str(), simple_string.size(), 0);
        auto root_dir = filesystem->getRootDir();
        auto file = root_dir->lookupFile(simple_file1).value();

        u32 rd_sz = file->read(buff, 100, 0);

        ASSERT_EQ(wrt_sz, simple_string.size());
        ASSERT_EQ(wrt_sz, rd_sz);
        ASSERT_STREQ(buff, simple_string.c_str());
    }

    filesystem->flush();
}

TEST(FileTest, SimpleWrite) {
    {
        char buff[100] = {0};
        char simple_string[] = "hello world!";
        u32 str_len = strlen(simple_string);
        auto root_dir = filesystem->getRootDir();
        auto file = root_dir->lookupFile(simple_file2).value();
        file->truncate(str_len);
        u32 wrt_sz = file->write(simple_string, str_len, 0);
        file->sync(true);
        filesystem->flush();
        TestFsEnv::reMount();

        auto rd_sz = readFile(simple_file2, buff, 100, 0);
        ASSERT_EQ(wrt_sz, str_len);
        ASSERT_EQ(wrt_sz, rd_sz);
        ASSERT_STREQ(buff, simple_string);
    }

    filesystem->flush();
}

TEST(FileTest, ExceedRW) {
    {
        auto root_dir = filesystem->getRootDir();
        auto file = root_dir->lookupFile(simple_file1).value();
        char buf;
        u32 impossible_offset = 1000;
        ASSERT_EQ(file->read(&buf, 1, impossible_offset), 0);
        ASSERT_EQ(file->write(&buf, 1, impossible_offset), 1);
    }

    filesystem->flush();
}

TEST(FileTest, LargeRead) {
    u32 clus_size = fat32::bytesPerClus(filesystem->bpb());
    u32 clus_cnt = 10;
    std::vector<u8> clus_content(clus_size, 0xdb);
    {
        for (u32 i = 0; i < clus_cnt; i++) {
            auto wrt_sz = writeFile(simple_file1, (const char *) &clus_content[0], clus_size, i * clus_size);
            ASSERT_EQ(wrt_sz, clus_size);
        }
        auto root_dir = filesystem->getRootDir();
        auto file = root_dir->lookupFile(simple_file1).value();

        for (u32 i = 0; i < clus_cnt; i++) {
            std::vector<u8> buffer(clus_size, 0);
            auto read_sz = file->read((char *) &buffer[0], clus_size, i * clus_size);
            ASSERT_EQ(read_sz, clus_size);
            ASSERT_EQ(buffer, clus_content);
        }
    }

    filesystem->flush();
}

TEST(FileTest, LargeWrite) {
    u32 clus_size = fat32::bytesPerClus(filesystem->bpb());
    u32 clus_cnt = 10;
    std::vector<u8> clus_content(clus_size, 0xef);
    {
        auto root_dir = filesystem->getRootDir();
        auto file = root_dir->lookupFile(simple_file2).value();
        for (u32 i = 0; i < clus_cnt; i++) {
            auto wrt_sz = file->write((const char *) &clus_content[0], clus_size, i * clus_size);
            ASSERT_EQ(wrt_sz, clus_size);
        }
        file->sync(true);
        filesystem->flush();
        TestFsEnv::reMount();

        for (u32 i = 0; i < clus_cnt; i++) {
            std::vector<u8> buffer(clus_size, 0);
            auto read_sz = readFile(simple_file2, (char *) &buffer[0], clus_size, i * clus_size);
            ASSERT_EQ(read_sz, clus_size);
            ASSERT_EQ(buffer, clus_content);
        }
    }

    filesystem->flush();
}

TEST(FileTest, SyncWithoutMeta) {
    {
        auto origin_stat = readFileStat(simple_file3).value();

        // perform read and write to change meta info and then sync the disk.
        const u32 buf_size = 123;
        char tmp_buf[buf_size];
        auto root_dir = filesystem->getRootDir();
        auto file = root_dir->lookupFile(simple_file3).value();
        ASSERT_EQ(file->write(tmp_buf, buf_size, 0), buf_size);
        ASSERT_EQ(file->read(tmp_buf, buf_size, 0), buf_size);
        file->sync(false);
        filesystem->flush();
        TestFsEnv::reMount();

        auto changed_stat = readFileStat(simple_file3).value();
        ASSERT_EQ(changed_stat.st_size, 0);
        ASSERT_EQ(unixTsCmp(origin_stat.st_atim, changed_stat.st_atim), 0);
        ASSERT_EQ(unixTsCmp(origin_stat.st_mtim, changed_stat.st_mtim), 0);
        ASSERT_EQ(unixTsCmp(origin_stat.st_ctim, changed_stat.st_ctim), 0);

        // recover
        file->truncate(0);
    }

    filesystem->flush();
}

TEST(FileTest, SyncWithMeta) {
    {
        auto origin_stat = readFileStat(simple_file3).value();
        sleep(2); // fat32 time field has two seconds deviation

        // perform read and write to change meta info and then sync the disk.
        const u32 buf_size = 321;
        char tmp_buf[buf_size];
        auto root_dir = filesystem->getRootDir();
        auto file = root_dir->lookupFile(simple_file3).value();
        ASSERT_EQ(file->write(tmp_buf, buf_size, 0), buf_size);
        ASSERT_EQ(file->read(tmp_buf, buf_size, 0), buf_size);
        file->sync(true);
        filesystem->flush();
        TestFsEnv::reMount();

        auto changed_stat = readFileStat(simple_file3).value();
        ASSERT_EQ(changed_stat.st_size, buf_size);
        ASSERT_EQ(unixTsCmp(origin_stat.st_atim, changed_stat.st_atim), 0); // fat32 access time only records date
        ASSERT_EQ(unixTsCmp(origin_stat.st_ctim, changed_stat.st_ctim), 0); // can not be changed
        ASSERT_EQ(unixTsCmp(origin_stat.st_mtim, changed_stat.st_mtim), -1);

        // recover
        file->truncate(0);
    }

    filesystem->flush();
}

// todo:
TEST(FileTest, SyncDir) {
    GTEST_SKIP();
}

// todo:
TEST(FileTest, SyncDeleted) {
    GTEST_SKIP();
}

TEST(FileTest, Truncate) {
    u32 clus_size = fat32::bytesPerClus(filesystem->bpb());
    {
        auto root = filesystem->getRootDir();
        auto file = root->lookupFile(empty_file).value();
        struct stat file_stat;

        // expand too big to cause failure
        u32 impossible_size = 4294967295; // U32::max
        ASSERT_FALSE(file->truncate(impossible_size));
        file->sync(true);
        filesystem->flush();
        TestFsEnv::reMount();
        file_stat = readFileStat(empty_file).value();
        ASSERT_EQ(file_stat.st_size, 0);

        // expand
        u32 expand_size = clus_size * 2 + 1;
        file->truncate(expand_size);
        file->sync(true);
        filesystem->flush();
        TestFsEnv::reMount();
        file_stat = readFileStat(empty_file).value();
        ASSERT_EQ(file_stat.st_size, expand_size);

        // shrink
        u32 shrink_size = clus_size;
        file->truncate(shrink_size);
        file->sync(true);
        filesystem->flush();
        TestFsEnv::reMount();
        file_stat = readFileStat(empty_file).value();
        ASSERT_EQ(file_stat.st_size, shrink_size);

        // clear
        file->truncate(0);
        file->sync(true);
        filesystem->flush();
        TestFsEnv::reMount();
        file_stat = readFileStat(empty_file).value();
        ASSERT_EQ(file_stat.st_size, 0);
    }

    filesystem->flush();
}

TEST(FileTest, SetTime) {
    auto origin_stat = readFileStat(simple_file1).value();

    auto root = filesystem->getRootDir();
    auto file = root->lookupFile(simple_file1).value();
    fat32::FatTimeStamp2 dos_ts = fat32::getCurDosTs2();
    // increase a year
    u16 year = ((dos_ts.ts.date >> 9) & 0b1111111) + 1;
    dos_ts.ts.date = ((dos_ts.ts.date & (0 << 9)) | (year << 9));

    file->setWrtTime(dos_ts.ts);
    file->setAccTime(dos_ts.ts);
    file->setCrtTime(dos_ts);
    file->sync(true);
    filesystem->flush();
    TestFsEnv::reMount();

    auto after_stat = readFileStat(simple_file1).value();
    ASSERT_EQ(unixTsCmp(origin_stat.st_atim, after_stat.st_atim), -1);
    ASSERT_EQ(unixTsCmp(origin_stat.st_ctim, after_stat.st_ctim), -1);
    ASSERT_EQ(unixTsCmp(origin_stat.st_mtim, after_stat.st_mtim), -1);
}

TEST(FileTest, MarkDeleted) {
    auto root = filesystem->getRootDir();
    // delete a file and directory
    auto file = root->lookupFile(simple_file2).value();
    auto dir = root->lookupFile(simple_dir2).value();
    file->markDeleted();
    dir->markDeleted();
    file->sync(true);
    dir->sync(true);
    filesystem->flush();
    TestFsEnv::reMount();

    errno = 0;
    ASSERT_EQ(open(simple_file2, O_RDONLY), -1);
    ASSERT_EQ(errno, ENOENT);
    errno = 0;
    ASSERT_EQ(open(simple_dir2, O_DIRECTORY), -1);
    ASSERT_EQ(errno, ENOENT);
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
