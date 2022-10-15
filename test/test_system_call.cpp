#include "gtest/gtest.h"

TEST(normal, success) {
    EXPECT_EQ(1, 1);
}

TEST(normal, failed) {
    EXPECT_EQ(1, 2);
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

