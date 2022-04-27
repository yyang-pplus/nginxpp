#pragma once

#include <string>
#include <utility>


namespace cxxopts {

class Options;
class ParseResult;

}


namespace nginxpp {

struct ServerOptions {
    std::string base_mount_dir;
    int port{};

    static constexpr bool tcp_nodelay = false;
    static constexpr int listen_backlog = 10;
};

void AddServerOptions(cxxopts::Options &options) noexcept;

[[nodiscard]]
ServerOptions HandleServerOptions(const cxxopts::ParseResult &parsed_options) noexcept;


class Socket {
public:
    static constexpr int INVALID_SOCKET = -1;

    explicit Socket(const int socket_fd) noexcept : m_socket_fd(socket_fd) {
    }
    ~Socket() noexcept;
    Socket(const Socket &) = delete;
    Socket &operator=(const Socket &) = delete;
    Socket(Socket &&other) noexcept :
        Socket(std::exchange(other.m_socket_fd, INVALID_SOCKET)) {
    }
    //Note: other may live longer than you thought, thus so may this m_socket_fd
    Socket &operator=(Socket &&other) noexcept {
        if (this != &other) {
            std::swap(m_socket_fd, other.m_socket_fd);
        }
        return *this;
    }

    [[nodiscard]]
    operator int() const noexcept {
        return m_socket_fd;
    }

private:
    int m_socket_fd = INVALID_SOCKET;
};


class HttpServer {
public:
    explicit HttpServer(const ServerOptions &options);
    ~HttpServer() noexcept = default;
    HttpServer(const HttpServer &) = delete;
    HttpServer &operator=(const HttpServer &) = delete;
    HttpServer(HttpServer &&) noexcept = default;
    HttpServer &operator=(HttpServer &&) noexcept = default;

    [[nodiscard]]
    bool Run() const noexcept;

private:
    void greet() const noexcept;

    ServerOptions m_options;
    Socket m_socket;
};


namespace internal {

[[nodiscard]]
Socket createServerSocket(const ServerOptions &options);

[[nodiscard]]
int getPort(const Socket &socket);

}//namespace internal

}//namespace nginxpp
