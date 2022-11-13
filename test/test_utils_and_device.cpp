#include <unistd.h>
#include <cstring>
#include <cerrno>

#include "gtest/gtest.h"

#include "config.h"
#include "device.h"
#include "util.h"
#include "common.h"

u32 sector_num = 64;
u64 regular_file_sz = SECTOR_SIZE * sector_num; // 32KB
const char message[] = "hello, world!";
bool enable_loop = false;

class TestUtilEnv : public testing::Environment {
public:
    ~TestUtilEnv() override = default;

    void SetUp() override {
        add_regular_file(regular_file, regular_file_sz);
        enable_loop = add_loop_file(loop_name, regular_file);
        if (enable_loop) {
            printf("loop enabled: %s.\n", loop_name);
        } else {
            printf("loop disabled.\n");
        }
    }

    void TearDown() override {
        if (enable_loop) {
            rm_loop_file(loop_name);
        }
        ASSERT_EQ(unlink(regular_file), 0);
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
    device::LinuxFileDriver linux_file_driver(regular_file, SECTOR_SIZE);
    testRegularRWOnDevice(linux_file_driver);
}

TEST(LinuxFileDriverTest, BlkDevRW) {
    if (!enable_loop) {
        GTEST_SKIP();
    }

    device::LinuxFileDriver linux_file_driver(loop_name, SECTOR_SIZE);
    testBlkDevRWOnDevice(linux_file_driver);
}

/**
 * SectorTest
 * */
TEST(SectorTest, RWFromOffset) {
    device::LinuxFileDriver linux_file_driver(regular_file, SECTOR_SIZE);
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
    device::LinuxFileDriver linux_file_driver(regular_file, SECTOR_SIZE);
    auto w_sector = linux_file_driver.readSector(0).value();
    ASSERT_DEATH(w_sector->write_ptr(SECTOR_SIZE), ".*");
}

TEST(SectorTest, SectorDestructor) {
    device::LinuxFileDriver linux_file_driver(regular_file, SECTOR_SIZE);
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
    device::LinuxFileDriver linuxFileDriver(regular_file, SECTOR_SIZE);
    device::CacheManager cacheManager(linuxFileDriver);
    auto sec1 = cacheManager.readSector(0);
    auto sec2 = cacheManager.readSector(0);
    ASSERT_EQ(sec1, sec2);
}

TEST(CacheManagerTest, LRU) {
    device::LinuxFileDriver linuxFileDriver(regular_file, SECTOR_SIZE);
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
    device::LinuxFileDriver linuxFileDriver(regular_file, SECTOR_SIZE);
    device::CacheManager cacheManager(linuxFileDriver);
    testRegularRWOnDevice(cacheManager);
}

TEST(CacheManagerTest, BlkDevRW) {
    if (!enable_loop) {
        GTEST_SKIP();
    }

    device::LinuxFileDriver linuxFileDriver(regular_file, SECTOR_SIZE);
    device::CacheManager cacheManager(linuxFileDriver);
    testBlkDevRWOnDevice(cacheManager);
}

TEST(IconvTest, Utf8AndGbk) {
    util::string_utf8 utf8_str = "\xe4\xbd\xa0\xe5\xa5\xbd,\xe4\xb8\x96\xe7\x95\x8c!"; // encoding of "你好,世界!" in utf8
    util::string_gbk gbk_str = "\xc4\xe3\xba\xc3,\xca\xc0\xbd\xe7!"; // encoding of "你好,世界!" in gbk
    util::string_utf8 incomplete_utf8_str = "\xe4\xbd\xa0\xe5\xa5";
    util::string_gbk incomplete_gbk_str = "\xc4\xe3\xba";

    // from utf8 to gbk
    auto result1 = util::utf8ToGbk(utf8_str);
    ASSERT_TRUE(result1.has_value()) << strerror(errno);
    util::string_gbk parsed_gbk_str = result1.value();
    ASSERT_EQ(gbk_str, parsed_gbk_str);

    // from gbk to utf8
    auto result2 = util::gbkToUtf8(gbk_str);
    ASSERT_TRUE(result2.has_value()) << strerror(errno);
    util::string_utf8 parsed_utf8_str = result2.value();
    ASSERT_EQ(utf8_str, parsed_utf8_str);

    // error
    errno = 0;
    ASSERT_FALSE(util::utf8ToGbk(incomplete_utf8_str).has_value());
    ASSERT_EQ(errno, EINVAL);
    errno = 0;
    ASSERT_FALSE(util::gbkToUtf8(incomplete_gbk_str).has_value());
    ASSERT_EQ(errno, EINVAL);
}

TEST(IconvTest, Utf8AndUtf16) {
    using namespace std::string_literals;
    util::string_utf8 utf8_str = "\xe4\xbd\xa0\xe5\xa5\xbd,\xe4\xb8\x96\xe7\x95\x8c!"; // encoding of "你好,世界!" in utf8
    util::string_utf16 utf16_str = "`O}Y,\x00\x16NLu!\x00"s; // encoding of "你好,世界!" in utf16
    util::string_utf8 incomplete_utf8_str = "\xe4\xbd\xa0\xe5\xa5";
    util::string_utf16 incomplete_utf16_str = "`O}"s;

    // from utf8 to utf16
    auto result1 = util::utf8ToUtf16(utf8_str);
    ASSERT_TRUE(result1.has_value()) << strerror(errno);
    util::string_utf16 parsed_utf16_str = result1.value();
    ASSERT_EQ(utf16_str, parsed_utf16_str);

    // from utf16 to utf8
    auto result2 = util::utf16ToUtf8(utf16_str);
    ASSERT_TRUE(result2.has_value()) << strerror(errno);
    util::string_utf8 parsed_utf8_str = result2.value();
    ASSERT_EQ(utf8_str, parsed_utf8_str);

    // error
    errno = 0;
    ASSERT_FALSE(util::utf8ToUtf16(incomplete_utf8_str).has_value());
    ASSERT_EQ(errno, EINVAL);
    errno = 0;
    ASSERT_FALSE(util::utf16ToUtf8(incomplete_utf16_str).has_value());
    ASSERT_EQ(errno, EINVAL);
}

// todo: check google test.

int main(int argc, char **argv) {
    testing::AddGlobalTestEnvironment(new TestUtilEnv);
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}