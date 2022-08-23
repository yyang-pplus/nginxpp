#include <nginxpp/args.hpp>
#include <nginxpp/exception.hpp>
#include <nginxpp/server.hpp>


using namespace nginxpp;


namespace {

[[nodiscard]] inline auto buildOptions() noexcept {
    auto options = CreateBaseOptions();

    AddServerOptions(options);

    return options;
}

[[nodiscard]] inline auto
handleOptions(cxxopts::Options &options, const int argc, const char *argv[]) noexcept {
    const auto results = [&]() {
        try {
            return options.parse(argc, argv);
        } catch (const cxxopts::option_not_exists_exception &e) {
            std::cerr << e.what() << std::endl;
        } catch (const cxxopts::OptionException &e) {
            std::cerr << e.what() << std::endl;
        }
        exit(EXIT_FAILURE);
    }();

    HandleBaseOptions(options, results);

    return HandleServerOptions(results);
}

[[nodiscard]] inline constexpr auto toExitCode(const bool success) noexcept {
    return success ? EXIT_SUCCESS : EXIT_FAILURE;
}

} // namespace


int main(int argc, const char *argv[]) {
    auto options = buildOptions();
    const auto server_options = handleOptions(options, argc, argv);

    auto server = [&server_options]() {
        try {
            return HttpServer {server_options};
        } catch (const ServerException &e) {
            std::cerr << e.what() << std::endl;
        } catch (const SocketException &e) {
            std::cerr << e.what() << std::endl;
        }
        exit(EXIT_FAILURE);
    }();

    return toExitCode(server.Run());
}
