#include <fcntl.h>
#include <sys/types.h>
#include <linux/loop.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>

#include "gtest/gtest.h"

#include "config.h"
#include "device.h"
#include "util.h"

char regular_file[] = "regular_file";
char loop_name[512];
u64 regular_file_sz = SECTOR_SIZE * 64; // 32KB

class TestFsEnv : public testing::Environment {
public:
    TestFsEnv() : enable_loop(false) {}

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

    bool enable_loop;
};

TEST(LRUCacheMapTest, BasicUsage) {
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

TEST(LinuxFileDriverTest, RegularRW) {
    device::LinuxFileDriver linux_file_driver(regular_file);
    auto sector = linux_file_driver.readSector(0).value();
    u8 *ptr = (u8 *) sector->write_ptr(0);
}

// TODO: use a loop file as /dev/sdb1 to prevent further issues!
TEST(LinuxFileDriverTest, BlkDevRW) {
}

int main(int argc, char **argv) {
    testing::AddGlobalTestEnvironment(new TestFsEnv);
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}