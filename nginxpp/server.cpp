#include <nginxpp/server.hpp>

#include <atomic>
#include <iostream>
#include <thread>

#include <csignal>
#include <errno.h>
#include <string.h>

#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cxxopts.hpp>

#include <nginxpp/exception.hpp>


using std::string_literals::operator""s;
using namespace nginxpp;


namespace {

std::atomic<int> g_signal{0};

extern "C" void signalHandler(int signal) {
    g_signal = signal;
}

template<typename... Args>
inline constexpr void setSocketOption(Args &&... args) {
    if (setsockopt(std::forward<Args>(args)...) == -1) {
        throw SocketException("Failed to setsockopt(): "s + strerror(errno));
    }
}

[[nodiscard]]
auto getPort(const sockaddr_storage &address) {
    if (address.ss_family == AF_INET) {
        return ntohs(reinterpret_cast<const sockaddr_in *>(&address)->sin_port);
    } else if (address.ss_family == AF_INET6) {
        return ntohs(reinterpret_cast<const sockaddr_in6 *>(&address)->sin6_port);
    } else {
        throw SocketException("Failed to get port: Unsupported address family");
    }
}

[[nodiscard]]
gsl::not_null<const void *> locateInternetAddress(const sockaddr_storage &address) {
    if (address.ss_family == AF_INET) {
        return &(reinterpret_cast<const sockaddr_in *>(&address)->sin_addr);
    } else if (address.ss_family == AF_INET6) {
        return &(reinterpret_cast<const sockaddr_in6 *>(&address)->sin6_addr);
    } else {
        throw SocketException("Failed to locate internet address: Unsupported address family");
    }
}

template<typename Function, typename... Args>
[[nodiscard]]
inline constexpr auto handleEINTR(const Function func, Args &&... args) noexcept {
    Expects(func);

    int result{};
    do {
        result = func(std::forward<Args>(args)...);
    } while (result < 0 and errno == EINTR);

    return result;
}

[[nodiscard]]
inline auto pollOne(pollfd &pfd, const std::chrono::milliseconds &timeout) noexcept {
    return handleEINTR(poll, &pfd, 1, timeout.count());
}

inline auto hasConnectionRequest(pollfd &pfd) noexcept {
    return 0 != pollOne(pfd, ServerOptions::accept_timeout);
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
        Socket sock{socket(p->ai_family, p->ai_socktype, p->ai_protocol)};
        if (sock == Socket::INVALID_SOCKET) {
            errors << "Failed to create socket: " << strerror(errno) << '\n';
            continue;
        }

        if (fcntl(sock, F_SETFD, FD_CLOEXEC) == -1) {
            continue;
        }

        setSocketOption(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
        if (ServerOptions::tcp_nodelay) {
            setSocketOption(sock, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(yes));
        }

        if (bind(sock, p->ai_addr, p->ai_addrlen) == -1) {
            errors << "Failed to bind(): " << strerror(errno) << '\n';
            continue;
        }

        if (listen(sock, ServerOptions::listen_backlog) == -1) {
            throw SocketException("Failed to listen(): "s + strerror(errno));
        }

        return sock;
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

    return ::getPort(address);
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


Socket::~Socket() noexcept {
    if (m_socket_fd != INVALID_SOCKET) {
        close(std::exchange(m_socket_fd, INVALID_SOCKET));
    }
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

void HttpServer::greet() const noexcept {
    std::cout << R"(
 _ __   __ _(_)
| '_ \ / _` | | '_ \\ \/ / '_ \| '_ \
| | | | (_| | | | | |>  <| |_) | |_) |
|_| |_|\__, |_|_| |_/_/\_\ .__/| .__/
       |___/             |_|   |_|      starting up.
)" <<
    "Listening on port: " << m_options.port << std::endl;
}


bool HttpServer::Run() const noexcept {
    greet();

    (void) std::signal(SIGINT, signalHandler);  // Handle 'Ctrl+c'
    (void) std::signal(SIGQUIT, signalHandler); // Handle 'Ctrl+\'

    sockaddr_storage their_address{};
    char address_buffer[INET6_ADDRSTRLEN] = {};
    pollfd server_pfd;
    server_pfd.fd = m_socket;
    server_pfd.events = POLLIN;
    for (;;) {
        if (g_signal) {
            std::cout << "Caught signal " << strsignal(g_signal) <<
                '(' << g_signal << "), shutting down..." << std::endl;
            break;
        }

        if (not hasConnectionRequest(server_pfd))
            continue;

        socklen_t address_size = sizeof(their_address);
        Socket sock{accept(m_socket, reinterpret_cast<sockaddr *>(&their_address), &address_size)};
        if (sock == Socket::INVALID_SOCKET) {
            if (errno == EMFILE) {
                std::cerr << "Open file descriptors limit reached. Waiting for available spots." << std::endl;
                std::this_thread::sleep_for(ServerOptions::accept_timeout);
                continue;
            }

            std::cerr << "Failed to accept(): " << strerror(errno) << std::endl;
            return false;
        }

        inet_ntop(their_address.ss_family, locateInternetAddress(their_address), address_buffer, sizeof(address_buffer));
        onAccept(std::move(sock), address_buffer, getPort(their_address));
    }

    return true;
}

void HttpServer::onAccept(Socket sock, const gsl::not_null<gsl::czstring> address, const int port) const noexcept {
    static std::atomic<unsigned> session_created{};

    Expects(sock != Socket::INVALID_SOCKET);

    const auto session_id = session_created++;
    std::cout << "Accepted new connection from: " << address <<
        "; Port: " << port <<
        "; FD: " << sock <<
        "; Session: " << session_id << std::endl;

    timeval timeout{};
    timeout.tv_sec = ServerOptions::read_timeout.count();
    timeout.tv_usec = 0;
    setSocketOption(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    timeout.tv_sec = ServerOptions::write_timeout.count();
    timeout.tv_usec = 0;
    setSocketOption(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
}

}//namespace nginxpp
