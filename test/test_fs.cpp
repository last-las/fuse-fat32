#include "gtest/gtest.h"

// TODO: use `mkfs.fat` to create a fat32, test fs structure on it.
class TestFsEnv : public testing::Environment {
public:
    void SetUp() override {
    }

    void TearDown() override {
    }
};


int main(int argc, char **argv) {
    testing::AddGlobalTestEnvironment(new TestFsEnv);
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
