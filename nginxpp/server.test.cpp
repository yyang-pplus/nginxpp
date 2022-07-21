#include <nginxpp/server.hpp>

#include <gtest/gtest.h>

#include <nginxpp/exception.hpp>


using namespace nginxpp;


namespace {

[[nodiscard]]
inline auto createServerOptions(const int port) {
    ServerOptions options;
    options.port = port;
    return options;
}

}


TEST(SocketTests, ThrowIfGivenInvalidSocketFd) {
    ASSERT_THROW(Socket{-1}, SocketException);
}


TEST(HttpServerTests, NoThrowIfGivenValidPort) {
    const auto options = createServerOptions(0);
    ASSERT_NO_THROW(HttpServer{options});
}

TEST(HttpServerTests, ThrowIfGivenInvalidPort) {
    const auto options = createServerOptions(-1);
    ASSERT_THROW(HttpServer{options}, SocketException);
}
