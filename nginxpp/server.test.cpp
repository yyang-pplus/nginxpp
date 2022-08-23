#include <nginxpp/server.hpp>

#include <gtest/gtest.h>

#include <nginxpp/exception.hpp>


using namespace nginxpp;


namespace {

[[nodiscard]] inline auto createServerOptions(const int port, std::string root_dir = ".") {
    ServerOptions options;
    options.base_mount_dir = root_dir;
    options.port = port;
    return options;
}

} // namespace


TEST(HttpServerTests, NoThrowIfGivenValidPort) {
    const auto options = createServerOptions(0);
    ASSERT_NO_THROW(HttpServer {options});
}

TEST(HttpServerTests, ThrowIfGivenPathNotExists) {
    const auto options = createServerOptions(0, "no_such_path");
    ASSERT_THROW(HttpServer {options}, ServerException);
}

TEST(HttpServerTests, ThrowIfGivenInvalidPort) {
    const auto options = createServerOptions(Socket::INVALID_SOCKET);
    ASSERT_THROW(HttpServer {options}, SocketException);
}

TEST(HttpServerTests, ThrowIfBindOnTheSamePortTwice) {
    auto options = createServerOptions(0);
    const auto socket = internal::createServerSocket(options);
    options.port = internal::getPort(socket);
    ASSERT_THROW(HttpServer {options}, SocketException);
}
