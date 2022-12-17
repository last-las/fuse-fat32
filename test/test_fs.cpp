#include "gtest/gtest.h"
#include "memory"
#include <sys/mount.h>

#include "common.h"
#include "fs.h"

std::unique_ptr<fs::FAT32fs> filesystem;
const char simple_file1[] = "short1.txt";
const char simple_file2[] = "short2.txt";
const char simple_file3[] = "short3.txt";
const char simple_file4[] = "short4.txt";
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
        crtFileOrExist(simple_file4);
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

class FsTest : public ::testing::Test {
public:
    ~FsTest() override {
        /**
         * flush() is called after each test case to make sure:
         * 1. there are no cached file objects, which might influence the next test case if it invokes system calls
         * before looking up a file object(e.g. create a file and then look up root dir);
         * 2. there are no cached sector objects, which means the content of them has been written back to the disk.
         *
         * sync() is called to make sure the content has been flushed to the disk by linux kernel.
         * */
        filesystem->flush();
        sync();
    }
};

class FAT32fsTest : public FsTest {
};

class FileTest : public FsTest {
};

class DirTest : public FsTest {
};


TEST_F(FAT32fsTest, RootDir) {
    auto root = filesystem->getRootDir();
    ASSERT_EQ(root->ino(), fs::KRootDirIno);
}

TEST_F(FAT32fsTest, GetFileByIno) {
    GTEST_SKIP();
}

TEST_F(FAT32fsTest, OpenFileByIno) {
    GTEST_SKIP();
}

TEST_F(FileTest, SimpleRead) {
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

TEST_F(FileTest, SimpleWrite) {
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

TEST_F(FileTest, ExceedRW) {
    auto root_dir = filesystem->getRootDir();
    auto file = root_dir->lookupFile(simple_file1).value();
    char buf;
    u32 impossible_offset = 1000;
    ASSERT_EQ(file->read(&buf, 1, impossible_offset), 0);
    ASSERT_EQ(file->write(&buf, 1, impossible_offset), 1);
}

TEST_F(FileTest, LargeRead) {
    u32 clus_size = fat32::bytesPerClus(filesystem->bpb());
    u32 clus_cnt = 10;
    std::vector<u8> clus_content(clus_size, 0xdb);
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

TEST_F(FileTest, LargeWrite) {
    u32 clus_size = fat32::bytesPerClus(filesystem->bpb());
    u32 clus_cnt = 10;
    std::vector<u8> clus_content(clus_size, 0xef);
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

TEST_F(FileTest, SyncWithoutMeta) {
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

TEST_F(FileTest, SyncWithMeta) {
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

TEST_F(FileTest, Truncate) {
    u32 clus_size = fat32::bytesPerClus(filesystem->bpb());
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

TEST_F(FileTest, SetTime) {
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

TEST_F(FileTest, MarkDeleted) {
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

TEST_F(FileTest, RenameTargetExists) {
    const char buf[] = "test rename function";
    u32 buf_size = strlen(buf);

    // rename an exist file to another exist file
    auto root = filesystem->getRootDir();
    auto old_file = root->lookupFile(simple_file1).value();
    auto new_file = root->lookupFile(simple_file3).value();
    old_file->truncate(buf_size);
    ASSERT_EQ(old_file->write(buf, buf_size, 0), buf_size);
    old_file->exchangeMetaData(new_file);
    old_file->markDeleted();

    root->sync(true);
    new_file->sync(true);
    filesystem->flush();
    TestFsEnv::reMount();

    assert_file_content(simple_file3, buf, buf_size);
    assert_file_non_exist(simple_file1);
}

TEST_F(DirTest, crtShortFile) {
    const char non_exist_short_name[] = "crtFile.txt";
    const char buf[] = "create file tests";
    u32 buf_size = strlen(buf);
    auto root = filesystem->getRootDir();
    auto short_file = root->crtFile(non_exist_short_name).value();
    ASSERT_EQ(short_file->write(buf, buf_size, 0), buf_size);
    root->sync(false);
    short_file->sync(true);
    filesystem->flush();
    TestFsEnv::reMount();

    assert_file_content(non_exist_short_name, buf, buf_size);
}

TEST_F(DirTest, crtLongFile) {
    const char non_exist_long_name[] = "create_file_test_non_exist_long_name.txt";
    const char buf[] = "create file tests";
    u32 buf_size = strlen(buf);
    auto root = filesystem->getRootDir();
    auto long_file = root->crtFile(non_exist_long_name).value();
    ASSERT_EQ(long_file->write(buf, buf_size, 0), buf_size);
    long_file->sync(true);
    root->sync(true);
    filesystem->flush();
    TestFsEnv::reMount();

    assert_file_content(non_exist_long_name, buf, buf_size);
}

// DirTest::crtFile must pass before running this.
TEST_F(FileTest, RenameTargetNotExists) {
    const char *exist_name = simple_file4;
    const char non_exist_name[] = "rename_non_exists_file.txt";
    const char buf[] = "test rename function";
    u32 buf_size = strlen(buf);

    // rename an exist file to a non exist file
    auto root = filesystem->getRootDir();
    auto old_file = root->lookupFile(exist_name).value();
    auto new_file = root->crtFile(non_exist_name).value();
    ASSERT_EQ(old_file->write(buf, buf_size, 0), buf_size);
    old_file->exchangeMetaData(new_file);
    old_file->markDeleted();

    root->sync(true);
    new_file->sync(true);
    filesystem->flush();
    TestFsEnv::reMount();

    assert_file_content(non_exist_name, buf, buf_size);
    assert_file_non_exist(exist_name);
}

TEST_F(DirTest, CrtDir) {
    const char new_dir_name[] = "crtDir_new_name";
    auto root = filesystem->getRootDir();
    auto sub_dir = root->crtDir(new_dir_name).value();
    root->sync(false);
    sub_dir->sync(true);
    filesystem->flush();
    TestFsEnv::reMount();

    assert_dir_exist(new_dir_name);
}

TEST_F(DirTest, CrtDirAndWrite) {
    const char new_dir_name[] = "crtDirAndWrite_new_name";
    const char new_file_name[] = "crtDirAndWrite_new_file.txt";
    const char buffer[] = "hello world!";
    const u32 buf_sz = strlen(buffer);

    {
        auto root = filesystem->getRootDir();
        auto sub_dir = root->crtDir(new_dir_name).value();
        auto file = sub_dir->crtFile(new_file_name).value();
        ASSERT_EQ(file->write(buffer, buf_sz, 0), buf_sz);
    }
    filesystem->flush();
    TestFsEnv::reMount();

    std::string path_to_new_file = util::format_string("%s/%s", new_dir_name, new_file_name);
    assert_file_content(path_to_new_file.c_str(), buffer, buf_sz);
}

// todo: fix crt multiple..
TEST_F(DirTest, DelFile) {
    const char new_dir[] = "DelFile_new_dir";
    const char new_file_prefix[] = "DelFile_new_file_";
    int file_num = 20;
    auto root = filesystem->getRootDir();
    auto sub_dir = root->crtDir(new_dir).value();

    // create files
    for (u32 i = 0; i < file_num; i++) {
        auto new_file_name = util::format_string("%s%s", new_file_prefix, std::to_string(i).c_str());
        ASSERT_TRUE(sub_dir->crtFile(new_file_name.c_str()).has_value());
    }
    sub_dir->sync(true);
    filesystem->flush();
    TestFsEnv::reMount();
    // make sure exists
    for (int i = 0; i < file_num; i++) {
        auto new_file_path = util::format_string("%s/%s%s", new_dir, new_file_prefix, std::to_string(i).c_str());
        assert_file_exist(new_file_path.c_str());
    }

    // delete
    for (int i = file_num - 1; i >= 0; i--) {
        auto new_file_name = util::format_string("%s%s", new_file_prefix, std::to_string(i).c_str());
        ASSERT_TRUE(sub_dir->delFile(new_file_name.c_str()));
    }
    sub_dir->sync(true);
    filesystem->flush();
    TestFsEnv::reMount();
    // check the file is surely deleted on disk
    for (int i = 0; i < file_num; i++) {
        auto new_file_path = util::format_string("%s/%s%s", new_dir, new_file_prefix, std::to_string(i).c_str());
        assert_file_non_exist(new_file_path.c_str());
    }
}

TEST_F(DirTest, ListDir) {
    const char new_dir[] = "List_dir";
    const char new_file_prefix[] = "List_dir_new_file_";
    int file_num = 20;

    // create files
    create_dir(new_dir);
    for (u32 i = 0; i < file_num; i++) {
        auto new_file_path = util::format_string("%s/%s%s", new_dir, new_file_prefix, std::to_string(i).c_str());
        create_file(new_file_path.c_str());
    }
    sync();

    auto root = filesystem->getRootDir();
    auto sub_dir = std::dynamic_pointer_cast<fs::Directory>(root->lookupFile(new_dir).value());
    u64 entry_off = 0;
    for (u32 i = 0; i < file_num + 2; i++) {
        auto file = sub_dir->lookupFileByIndex(entry_off).value();
        if (i == 0) {
            ASSERT_STREQ(".", file->name());
        } else if (i == 1) {
            ASSERT_STREQ("..", file->name());
        } else {
            auto file_name = util::format_string("%s%s", new_file_prefix, std::to_string(i - 2).c_str());
            ASSERT_STREQ(file_name.c_str(), file->name());
        }
    }
    ASSERT_FALSE(sub_dir->lookupFileByIndex(entry_off).has_value());
}

TEST_F(DirTest, JudgeEmpty) {
    const char empty_dir[] = "empty";
    const char non_empty_dir[] = "non_empty";
    crtDirOrExist(empty_dir);
    crtDirOrExist(non_empty_dir);
    crtFileOrExist(util::format_string("%s/%s", non_empty_dir, "file.txt").c_str());
    sync();

    auto root_dir = filesystem->getRootDir();
    auto empty = std::dynamic_pointer_cast<fs::Directory>(root_dir->lookupFile(empty_dir).value());
    auto non_empty = std::dynamic_pointer_cast<fs::Directory>(root_dir->lookupFile(non_empty_dir).value());

    ASSERT_FALSE(root_dir->isEmpty());
    ASSERT_TRUE(empty->isEmpty());
    ASSERT_FALSE(non_empty->isEmpty());
}


int main(int argc, char **argv) {
    testing::AddGlobalTestEnvironment(new TestFsEnv);
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
