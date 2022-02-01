#include <psqlxx/args.hpp>

#include <gtest/gtest.h>


using namespace psqlxx;


TEST(ParseOptionsTests, UnrecognisedOptionsDonotThrow) {
    auto options = CreateBaseOptions();

    const char *argv[] = {"runner", "--no-such-option"};
    const auto argc = sizeof(argv) / sizeof(argv[0]);

    ASSERT_FALSE(ParseOptions(options, argc, argv));
}


TEST(ParseOptionsTests, MissingValueOptionsDonotThrow) {
    const std::string OPTION_NAME = "a-test-option";
    const std::string FULL_OPTION_NAME = "--" + OPTION_NAME;
    auto options = CreateBaseOptions();
    options.add_options()(OPTION_NAME, "A test option", cxxopts::value<int>());

    const char *argv[] = {"runner", FULL_OPTION_NAME.c_str()};
    const auto argc = sizeof(argv) / sizeof(argv[0]);

    ASSERT_FALSE(ParseOptions(options, argc, argv));
}
