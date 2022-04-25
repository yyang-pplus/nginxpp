#pragma once

#include <string>


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
    explicit Socket(const int socket_fd);
    ~Socket() noexcept;
    Socket(const Socket &) = delete;
    Socket &operator=(const Socket &) = delete;
    Socket(Socket &&) noexcept = default;
    Socket &operator=(Socket &&) noexcept = default;

private:
    int m_socket_fd = -1;
};


class HttpServer {
public:
    explicit HttpServer(const ServerOptions &options);

    bool Run() const noexcept {
        for (;;);
        return true;
    }

private:
    Socket m_socket;
};

}//namespace nginxpp
