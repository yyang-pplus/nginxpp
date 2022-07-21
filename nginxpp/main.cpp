#include <nginxpp/args.hpp>


using namespace nginxpp;


namespace {

[[nodiscard]]
inline auto buildOptions() {
    auto options = CreateBaseOptions();

    return options;
}

[[nodiscard]]
inline auto
handleOptions(cxxopts::Options &options, const int argc, const char *argv[]) {
    const auto results = ParseOptions(options, argc, argv);
    if (not results) {
        exit(EXIT_FAILURE);
    }

    HandleBaseOptions(options, results.value());
}

[[nodiscard]]
inline constexpr auto toExitCode(const bool success) {
    return success ? EXIT_SUCCESS : EXIT_FAILURE;
}

}


int main(int argc, const char *argv[]) {
    auto options = buildOptions();

    handleOptions(options, argc, argv);
}
