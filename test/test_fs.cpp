#include <fcntl.h>
#include <sys/types.h>
#include <linux/loop.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>

#include "gtest/gtest.h"

#include "config.h"
#include "device.h"
#include "util.h"

char regular_file[] = "regular_file";
char loop_name[512];
u32 sector_num = 64;
u64 regular_file_sz = SECTOR_SIZE * sector_num; // 32KB
const char message[] = "hello, world!";
bool enable_loop = false;

class TestFsEnv : public testing::Environment {
public:
    ~TestFsEnv() override = default;

    void SetUp() override {
        add_regular_file();
        add_loop_file();
        if (enable_loop) {
            printf("loop enabled: %s.\n", loop_name);
        } else {
            printf("loop disabled.\n");
        }
    }

    void TearDown() override {
        if (enable_loop) {
            rm_loop_file();
        }
        ASSERT_EQ(unlink(regular_file), 0);
    }

private:
    void add_regular_file() {
        int fd = open(regular_file, O_CREAT, 0644);
        ASSERT_GT(fd, 0);
        ASSERT_EQ(truncate64(regular_file, regular_file_sz), 0);
        ASSERT_EQ(close(fd), 0);
    }

    // came from "man 4 loop"
    void add_loop_file() {
        // find a free loop device
        int loop_ctl_fd = open("/dev/loop-control", O_RDWR);
        if (loop_ctl_fd == -1) {
            perror("open: /dev/loop-control");
            return;
        }
        int dev_nr = ioctl(loop_ctl_fd, LOOP_CTL_GET_FREE);
        if (dev_nr == -1) {
            perror("ioctl-LOOP_CTL_GET_FREE");
            return;
        }
        sprintf(loop_name, "/dev/loop%d", dev_nr);
        int loop_fd = open(loop_name, O_RDWR);
        if (loop_fd == -1) {
            perror("open: loop_name");
            return;
        }

        // open `regular_file` as backend file
        int bck_fd = open(regular_file, O_RDWR);
        ASSERT_GT(bck_fd, 0);

        // loop
        if (ioctl(loop_fd, LOOP_SET_FD, bck_fd) == -1) {
            perror("ioctl-LOOP_SET_FD");
        }

        close(loop_fd);
        close(bck_fd);
        enable_loop = true;
    }

    void rm_loop_file() {
        int loop_fd = open(loop_name, O_RDONLY);
        if (loop_fd == -1) {
            perror("open: loop_name");
            return;
        }

        if (ioctl(loop_fd, LOOP_CLR_FD) == -1) {
            perror("ioctl-LOOP_CLR_FD");
            return;
        }
    }
};

/**
 * LRUCacheMapTest
 * */
TEST(LRUCacheMapTest, PutElement) {
    u32 max_size = 3;
    util::LRUCacheMap<u32, u32> lru_map(max_size);
    lru_map.put(1, 1);
    lru_map.put(2, 2);
    lru_map.put(3, 3);
    lru_map.put(4, 4);
    lru_map.put(2, 2);
    lru_map.put(5, 5);

    ASSERT_FALSE(lru_map.get(1).has_value());
    ASSERT_FALSE(lru_map.get(3).has_value());
    ASSERT_EQ(lru_map.get(4).value(), 4);
    ASSERT_EQ(lru_map.get(2).value(), 2);
    ASSERT_EQ(lru_map.get(5).value(), 5);
}

TEST(LRUCacheMapTest, GetElement) {
    u32 max_size = 4;
    util::LRUCacheMap<u32, u32> lru_map(max_size);
    lru_map.put(1, 1);
    lru_map.put(2, 2);
    lru_map.put(3, 3);
    lru_map.put(4, 4);
    lru_map.get(1);
    lru_map.get(2);
    lru_map.put(5, 5);
    lru_map.put(6, 6);

    ASSERT_FALSE(lru_map.get(3).has_value());
    ASSERT_FALSE(lru_map.get(4).has_value());
    ASSERT_EQ(lru_map.get(1).value(), 1);
    ASSERT_EQ(lru_map.get(2).value(), 2);
    ASSERT_EQ(lru_map.get(5).value(), 5);
    ASSERT_EQ(lru_map.get(6).value(), 6);
}

class TestObj {
public:
    TestObj() noexcept = default;
};

TEST(LRUCacheMapTest, SharedPtr) {
    u32 max_size = 3;
    util::LRUCacheMap<u32, std::shared_ptr<TestObj>> lru_map(max_size);
    std::shared_ptr<TestObj> testObj = std::make_shared<TestObj>();
    ASSERT_EQ(testObj.use_count(), 1);
    lru_map.put(1, testObj);
    ASSERT_EQ(testObj.use_count(), 2);
    // TODO: std::moved
}

void testRegularRWOnDevice(device::Device &device) {
    byte val = 0x66;
    for (u32 i = 0; i < sector_num; ++i) {
        // write sector
        auto w_sector = device.readSector(i).value();
        auto w_ptr = (byte *) w_sector->write_ptr(0);
        memset(w_ptr, val, SECTOR_SIZE);
        w_sector->sync();

        // read sector
        auto r_sector = device.readSector(i).value();
        auto r_ptr = (const byte *) r_sector->read_ptr(0);
        for (int j = 0; j < SECTOR_SIZE; ++j) {
            ASSERT_EQ(*r_ptr, val);
        }
    }
}

void testBlkDevRWOnDevice(device::Device &device) {
    byte val = 0x66;
    for (u32 i = 0; i < sector_num; ++i) {
        // write sector
        auto w_sector = device.readSector(i).value();
        auto w_ptr = (byte *) w_sector->write_ptr(0);
        memset(w_ptr, val, SECTOR_SIZE);
        w_sector->sync();

        // read sector
        auto r_sector = device.readSector(i).value();
        auto r_ptr = (const byte *) r_sector->read_ptr(0);
        for (int j = 0; j < SECTOR_SIZE; ++j) {
            ASSERT_EQ(*r_ptr, val);
        }
    }
}

/**
 * LinuxFileDriverTest
 * */
TEST(LinuxFileDriverTest, RegularRW) {
    device::LinuxFileDriver linux_file_driver(regular_file);
    testRegularRWOnDevice(linux_file_driver);
}

TEST(LinuxFileDriverTest, BlkDevRW) {
    if (!enable_loop) {
        GTEST_SKIP();
    }

    device::LinuxFileDriver linux_file_driver(loop_name);
    testBlkDevRWOnDevice(linux_file_driver);
}

/**
 * SectorTest
 * */
TEST(SectorTest, RWFromOffset) {
    device::LinuxFileDriver linux_file_driver(regular_file);
    auto off = 5;
    auto mess_len = strlen(message);
    auto w_sector = linux_file_driver.readSector(0).value();
    auto w_ptr = (byte *) w_sector->write_ptr(off);
    strncpy(w_ptr, message, mess_len);
    w_sector->sync();

    auto r_sector = linux_file_driver.readSector(0).value();
    auto r_ptr = (const byte *) w_sector->read_ptr(0);
    ASSERT_EQ(strncmp(message, r_ptr + off, mess_len), 0);
}

TEST(SectorTest, OffsetBeyondSectorSize) {
    device::LinuxFileDriver linux_file_driver(regular_file);
    auto w_sector = linux_file_driver.readSector(0).value();
    ASSERT_DEATH(w_sector->write_ptr(SECTOR_SIZE), ".*");
}

TEST(SectorTest, SectorDestructor) {
    device::LinuxFileDriver linux_file_driver(regular_file);
    auto mess_len = strlen(message);

    {
        auto w_sector = linux_file_driver.readSector(0).value();
        auto w_ptr = (byte *) w_sector->write_ptr(0);
        strncpy(w_ptr, message, mess_len);

        auto r_sector = linux_file_driver.readSector(0).value();
        auto r_ptr = (const byte *) r_sector->read_ptr(0);
        ASSERT_GT(strncmp(message, r_ptr, mess_len), 0);
    }

    auto r_sector = linux_file_driver.readSector(0).value();
    auto r_ptr = (const byte *) r_sector->read_ptr(0);
    ASSERT_EQ(strncmp(message, r_ptr, mess_len), 0);
}

/**
 * CacheManagerTest
 * */
TEST(CacheManagerTest, FetchSameSecTwice) {
    device::LinuxFileDriver linuxFileDriver(regular_file);
    device::CacheManager cacheManager(linuxFileDriver);
    auto sec1 = cacheManager.readSector(0);
    auto sec2 = cacheManager.readSector(0);
    ASSERT_EQ(sec1, sec2);
}

TEST(CacheManagerTest, LRU) {
    device::LinuxFileDriver linuxFileDriver(regular_file);
    device::CacheManager cacheManager(linuxFileDriver, 3);
    cacheManager.readSector(0);
    cacheManager.readSector(1);
    cacheManager.readSector(2);
    cacheManager.readSector(3);
    cacheManager.readSector(1);
    cacheManager.readSector(4);
    ASSERT_FALSE(cacheManager.contains(0));
    ASSERT_FALSE(cacheManager.contains(2));
    ASSERT_TRUE(cacheManager.contains(3));
    ASSERT_TRUE(cacheManager.contains(1));
    ASSERT_TRUE(cacheManager.contains(4));
}

TEST(CacheManagerTest, RegularRW) {
    device::LinuxFileDriver linuxFileDriver(regular_file);
    device::CacheManager cacheManager(linuxFileDriver);
    testRegularRWOnDevice(cacheManager);
}

TEST(CacheManagerTest, BlkDevRW) {
    if (!enable_loop) {
        GTEST_SKIP();
    }

    device::LinuxFileDriver linuxFileDriver(regular_file);
    device::CacheManager cacheManager(linuxFileDriver);
    testBlkDevRWOnDevice(cacheManager);
}

// todo: check google test.

int main(int argc, char **argv) {
    testing::AddGlobalTestEnvironment(new TestFsEnv);
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}