#include <nginxpp/string_utils.hpp>

#include <gtest/gtest.h>


using namespace nginxpp;
using namespace std::string_literals;


const auto PREFIX = "a_prefix"s;


TEST(StartsWithTests, ReturnFalseIfGiveEmptyString) {
    ASSERT_FALSE(StartsWith("", PREFIX));
}

TEST(StartsWithTests, ReturnTrueIfGiveEmptyPrefix) {
    ASSERT_TRUE(StartsWith(PREFIX, ""));
}

TEST(StartsWithTests, ReturnTrueIfGiveNullPrefix) {
    ASSERT_TRUE(StartsWith(PREFIX, {}));
}

TEST(StartsWithTests, ReturnTrueIfGiveRightPrefix) {
    ASSERT_TRUE(StartsWith(PREFIX, PREFIX));
}

TEST(StartsWithTests, ReturnFalseIfGiveWrongPrefix) {
    ASSERT_FALSE(StartsWith("some_string"s + PREFIX, PREFIX));
}
