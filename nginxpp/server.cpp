#include <nginxpp/server.hpp>

#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include <netdb.h>
#include <netinet/tcp.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <cxxopts.hpp>
#include <gsl/gsl>

#include <nginxpp/exception.hpp>


using std::string_literals::operator""s;
using namespace nginxpp;


namespace {

template<typename... Args>
inline constexpr void
setSocketOption(Args &&... args) {
    if (setsockopt(std::forward<Args>(args)...) == -1) {
        throw SocketException("Failed to setsockopt(): "s + strerror(errno));
    }
}

}//namespace


namespace nginxpp {

namespace internal {

Socket createServerSocket(const ServerOptions &options) {
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    const auto service = std::to_string(options.port);
    gsl::owner<addrinfo *> servinfo{};
    if (const auto status = getaddrinfo(nullptr, service.c_str(), &hints, &servinfo);
        status) {
        throw SocketException{"Failed to getaddrinfo(): "s + gai_strerror(status)};
    }
    // Only call freeaddrinfo() after a successful getaddrinfo()
    const auto servinfo_final = gsl::finally([servinfo]() {
        freeaddrinfo(servinfo);
    });

    const int yes = 1;
    std::stringstream errors;
    for (auto *p = servinfo; p; p = p->ai_next) {
        const auto socket_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (socket_fd == Socket::INVALID_SOCKET) {
            errors << "Failed to create socket: " << strerror(errno) << '\n';
            continue;
        }
        Socket s(socket_fd);

        if (fcntl(socket_fd, F_SETFD, FD_CLOEXEC) == -1) {
            continue;
        }

        setSocketOption(socket_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
        if (ServerOptions::tcp_nodelay) {
            setSocketOption(socket_fd, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(yes));
        }

        if (bind(socket_fd, p->ai_addr, p->ai_addrlen) == -1) {
            errors << "Failed to bind(): " << strerror(errno) << '\n';
            continue;
        }

        if (listen(socket_fd, ServerOptions::listen_backlog) == -1) {
            throw SocketException("Failed to listen(): "s + strerror(errno));
        }

        return s;
    }

    throw SocketException("Failed to create server socket: " + errors.str());
}

[[nodiscard]]
int getPort(const Socket &socket) {
    sockaddr_storage address{};
    socklen_t length = sizeof(address);
    if (getsockname(socket, reinterpret_cast<sockaddr *>(&address), &length)
        == -1) {
        throw SocketException("Failed to getsockname(): "s + strerror(errno));
    }

    if (address.ss_family == AF_INET) {
        return ntohs(reinterpret_cast<sockaddr_in *>(&address)->sin_port);
    } else if (address.ss_family == AF_INET6) {
        return ntohs(reinterpret_cast<sockaddr_in6 *>(&address)->sin6_port);
    } else {
        throw SocketException("Failed to get port: Unsupported address family");
    }
}

}//namespace internal


void AddServerOptions(cxxopts::Options &options) noexcept {
    options.add_options("Server")
    ("p,port",
     "port on which the server is to listen for connections from client applications.",
     cxxopts::value<int>()->default_value("19840"), "PORT")
    ("m,mount", "base directory that the server will mount on",
     cxxopts::value<std::string>()->default_value("./"), "DIR")
    ;
}

ServerOptions HandleServerOptions(const cxxopts::ParseResult &parsed_options) noexcept {
    ServerOptions options;

    options.base_mount_dir = parsed_options["mount"].as<std::string>();

    options.port = parsed_options["port"].as<int>();

    return options;
}


void Socket::close() noexcept {
    if (m_socket_fd != INVALID_SOCKET) {
        ::close(std::exchange(m_socket_fd, INVALID_SOCKET));
    }
}

Socket &Socket::operator=(Socket &&other) noexcept {
    if (this != &other) {
        close();
        m_socket_fd = std::exchange(other.m_socket_fd, INVALID_SOCKET);
    }

    return *this;
}


HttpServer::HttpServer(const ServerOptions &options):
    m_options(options),
    m_socket(internal::createServerSocket(options)) {
    Expects(m_socket != Socket::INVALID_SOCKET);
    if (m_options.port == 0) {
        m_options.port = internal::getPort(m_socket);
    }
    Ensures(m_options.port != 0);
}

}//namespace nginxpp
