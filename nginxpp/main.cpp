#include <nginxpp/args.hpp>
#include <nginxpp/server.hpp>


using namespace nginxpp;


namespace {

[[nodiscard]]
inline auto buildOptions() noexcept {
    auto options = CreateBaseOptions();

    AddServerOptions(options);

    return options;
}

[[nodiscard]]
inline auto
handleOptions(cxxopts::Options &options, const int argc, const char *argv[]) noexcept {
    try {
        const auto results = options.parse(argc, argv);

        HandleBaseOptions(options, results);

        return HandleServerOptions(results);
    } catch (const cxxopts::option_not_exists_exception &e) {
        std::cerr << e.what() << std::endl;
    } catch (const cxxopts::OptionException &e) {
        std::cerr << e.what() << std::endl;
    }
    exit(EXIT_FAILURE);
}

[[nodiscard]]
inline constexpr auto toExitCode(const bool success) noexcept {
    return success ? EXIT_SUCCESS : EXIT_FAILURE;
}

}


int main(int argc, const char *argv[]) {
    auto options = buildOptions();

    HttpServer server(handleOptions(options, argc, argv));
}
