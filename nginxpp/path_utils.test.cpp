#include <nginxpp/path_utils.hpp>

#include <gtest/gtest.h>


using namespace nginxpp;


TEST(GetFileSizeTest, ReturnNegativeIfFileNotExist) {
    EXPECT_EQ(-1, GetFileSize("no_such_file.txt"));
}

TEST(GetFileSizeTest, ReturnPositiveIfFileExist) {
    EXPECT_LE(0, GetFileSize("Makefile"));
}


TEST(StartsWithTest, ReturnTrueIfSamePath) {
    EXPECT_TRUE(StartsWith(std::filesystem::current_path(), std::filesystem::current_path()));
}

TEST(StartsWithTest, ReturnTrueIfPrefix) {
    const std::filesystem::path prefix = "/just/an/example";
    EXPECT_TRUE(StartsWith(prefix / "suffix.txt", prefix));
}

TEST(StartsWithTest, ReturnFalseIfNotPrefix) {
    const std::filesystem::path prefix = "/just/an/example";
    EXPECT_FALSE(StartsWith(prefix, prefix / "suffix"));
}
