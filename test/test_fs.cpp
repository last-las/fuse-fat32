#include "gtest/gtest.h"

#include "util.h"

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
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}