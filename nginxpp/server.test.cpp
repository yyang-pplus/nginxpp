#include <nginxpp/server.hpp>

#include <gtest/gtest.h>

#include <nginxpp/exception.hpp>


using namespace nginxpp;


namespace {

[[nodiscard]] inline auto createServerOptions(const int port) {
    ServerOptions options;
    options.port = port;
    return options;
}

} // namespace


TEST(HttpServerTests, NoThrowIfGivenValidPort) {
    const auto options = createServerOptions(0);
    ASSERT_NO_THROW(HttpServer {options});
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
