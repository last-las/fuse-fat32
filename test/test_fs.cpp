#include "gtest/gtest.h"
#include "memory"
#include <sys/mount.h>

#include "common.h"
#include "fs.h"

std::unique_ptr<fs::FAT32fs> filesystem;
const char simple_file1[] = "short1.txt";
const char simple_file2[] = "short2.txt";
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
        crtFileOrExist(long_name_file);
        sync();
    }

    static void recover() {
        ASSERT_EQ(chdir(".."), 0);
    }

    static void reMount() {
        chdir("..");

        if (umount(mnt_point) != 0) {
            printf("errno: %d %s\n", errno, strerror(errno));
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
        auto _ = filesystem->getRootDir();
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
