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
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <cxxopts.hpp>

#include <nginxpp/exception.hpp>


using std::string_literals::operator""s;
using namespace nginxpp;


namespace {

std::atomic<int> g_signal {0};

extern "C" void signalHandler(int signal) {
    g_signal = signal;
}

template<typename... Args>
inline constexpr void setSocketOption(Args &&...args) {
    if (setsockopt(std::forward<Args>(args)...) == -1) {
        throw SocketException("Failed to setsockopt(): "s + strerror(errno));
    }
}

[[nodiscard]] auto getPort(const sockaddr_storage &address) {
    if (address.ss_family == AF_INET) {
        return ntohs(reinterpret_cast<const sockaddr_in *>(&address)->sin_port);
    } else if (address.ss_family == AF_INET6) {
        return ntohs(reinterpret_cast<const sockaddr_in6 *>(&address)->sin6_port);
    } else {
        throw SocketException("Failed to get port: Unsupported address family");
    }
}

[[nodiscard]] gsl::not_null<const void *> locateInternetAddress(const sockaddr_storage &address) {
    if (address.ss_family == AF_INET) {
        return &(reinterpret_cast<const sockaddr_in *>(&address)->sin_addr);
    } else if (address.ss_family == AF_INET6) {
        return &(reinterpret_cast<const sockaddr_in6 *>(&address)->sin6_addr);
    } else {
        throw SocketException("Failed to locate internet address: Unsupported address family");
    }
}

template<typename Function, typename... Args>
[[nodiscard]] inline constexpr auto handleEINTR(const Function func, Args &&...args) noexcept {
    Expects(func);

    int result {};
    do {
        result = func(std::forward<Args>(args)...);
    } while (result < 0 and errno == EINTR);

    return result;
}

[[nodiscard]] inline auto pollOne(pollfd &pfd,
                                  const std::chrono::milliseconds &timeout) noexcept {
    return handleEINTR(poll, &pfd, 1, timeout.count());
}

[[nodiscard]] inline auto hasConnectionRequest(pollfd &pfd) noexcept {
    Expects(pfd.events == POLLIN);

    return 0 != pollOne(pfd, ServerOptions::accept_timeout);
}

[[nodiscard]] inline auto readyToRead(pollfd &pfd) noexcept {
    Expects(pfd.events == POLLIN);

    return 0 != pollOne(pfd, ServerOptions::read_timeout);
}

[[nodiscard]] inline auto read(const Socket &sock, std::string &buffer) noexcept {
    return handleEINTR(recv, sock, buffer.data(), buffer.size(), 0);
}

} //namespace


namespace nginxpp {

namespace internal {

Socket createServerSocket(const ServerOptions &options) {
    addrinfo hints {};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    const auto service = std::to_string(options.port);
    gsl::owner<addrinfo *> servinfo {};
    if (const auto status = getaddrinfo(nullptr, service.c_str(), &hints, &servinfo); status) {
        throw SocketException {"Failed to getaddrinfo(): "s + gai_strerror(status)};
    }
    // Only call freeaddrinfo() after a successful getaddrinfo()
    const auto servinfo_final = gsl::finally([servinfo]() {
        freeaddrinfo(servinfo);
    });

    const int yes = 1;
    std::stringstream errors;
    for (auto *p = servinfo; p; p = p->ai_next) {
        Socket sock {socket(p->ai_family, p->ai_socktype, p->ai_protocol)};
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

[[nodiscard]] int getPort(const Socket &socket) {
    sockaddr_storage address {};
    socklen_t length = sizeof(address);
    if (getsockname(socket, reinterpret_cast<sockaddr *>(&address), &length) == -1) {
        throw SocketException("Failed to getsockname(): "s + strerror(errno));
    }

    return ::getPort(address);
}

} //namespace internal


class SocketStream {
public:
    explicit SocketStream(Socket sock) noexcept;
    ~SocketStream() noexcept = default;
    SocketStream(const SocketStream &) = delete;
    SocketStream &operator=(const SocketStream &) = delete;
    SocketStream(SocketStream &&) noexcept = default;
    SocketStream &operator=(SocketStream &&) noexcept = default;

    [[nodiscard]] bool GetLine(std::string &out, const char delimiter = '\n');

private:
    static constexpr int buffer_size = 1024 * 4;

    Socket m_socket;
    pollfd m_poll_fd {};
    std::string m_buffer;
    int m_buffer_begin = 0;
    int m_buffer_end = 0;
    bool m_connect_closed = false;
};

SocketStream::SocketStream(Socket sock) noexcept :
    m_socket(std::move(sock)), m_buffer(buffer_size, 0) {
    Expects(m_socket != Socket::INVALID_SOCKET);

    timeval timeout {};
    timeout.tv_sec = ServerOptions::read_timeout.count();
    timeout.tv_usec = 0;
    setSocketOption(m_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    timeout.tv_sec = ServerOptions::write_timeout.count();
    timeout.tv_usec = 0;
    setSocketOption(m_socket, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

    m_poll_fd.fd = m_socket;
    m_poll_fd.events = POLLIN;
}

bool SocketStream::GetLine(std::string &out, const char delimiter) {
    out.clear();

    while (not m_connect_closed) {
        if (m_buffer_begin >= m_buffer_end) {
            if (not readyToRead(m_poll_fd)) {
                continue;
            }

            m_buffer_end = read(m_socket, m_buffer);
            if (m_buffer_end < 0) {
                throw SocketException("Failed to recv(): "s + strerror(errno));
            } else if (m_buffer_end == 0) {
                m_connect_closed = true;
                return not out.empty();
            }
            m_buffer_begin = 0;
        }

        const auto c = m_buffer[m_buffer_begin++];
        if (c == delimiter) {
            return true;
        }
        out.push_back(c);
    }

    return not m_connect_closed;
}


class Session {
public:
    Session(Socket sock, const gsl::not_null<gsl::czstring> address, const int port) noexcept;
    ~Session() noexcept = default;
    Session(const Session &) = delete;
    Session &operator=(const Session &) = delete;
    Session(Session &&) noexcept = default;
    Session &operator=(Session &&) noexcept = default;

    void Run() noexcept;

private:
    [[nodiscard]] auto &log(std::ostream &out = std::cout) const noexcept {
        return out << '[' << m_id << "] ";
    }

    static unsigned session_created;

    SocketStream m_stream;
    unsigned m_id {};
};

unsigned Session::session_created = 0;

Session::Session(Socket sock, const gsl::not_null<gsl::czstring> address, const int port) noexcept
    :
    m_stream(std::move(sock)),
    m_id(session_created++) {
    log() << "Accepted new connection from: " << address << "; Port: " << port
          << "; Session: " << m_id << std::endl;
}

void Session::Run() noexcept {
    std::string a_line;
    while (m_stream.GetLine(a_line)) {
        log() << a_line << std::endl;
    }

    log() << "Connection closed." << std::endl;
}


void AddServerOptions(cxxopts::Options &options) noexcept {
    // clang-format off
    options.add_options("Server")
    ("p,port", "port on which the server is to listen for connections from client applications.",
     cxxopts::value<int>()->default_value("19840"), "PORT")
    ("m,mount", "base directory that the server will mount on",
     cxxopts::value<std::string>()->default_value("./"), "DIR")
    ;
    // clang-format on
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


HttpServer::HttpServer(const ServerOptions &options) :
    m_options(options), m_socket(internal::createServerSocket(options)) {
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
)"
              << "Listening on port: " << m_options.port << std::endl;
}


bool HttpServer::Run() const noexcept {
    greet();

    (void)std::signal(SIGINT, signalHandler);  // Handle 'Ctrl+c'
    (void)std::signal(SIGQUIT, signalHandler); // Handle 'Ctrl+\'

    sockaddr_storage their_address {};
    char address_buffer[INET6_ADDRSTRLEN] = {};
    pollfd server_pfd;
    server_pfd.fd = m_socket;
    server_pfd.events = POLLIN;
    for (;;) {
        if (g_signal) {
            std::cout << "Caught signal " << strsignal(g_signal) << '(' << g_signal
                      << "), shutting down..." << std::endl;
            break;
        }

        if (not hasConnectionRequest(server_pfd))
            continue;

        socklen_t address_size = sizeof(their_address);
        Socket sock {
            accept(m_socket, reinterpret_cast<sockaddr *>(&their_address), &address_size)};
        if (sock == Socket::INVALID_SOCKET) {
            if (errno == EMFILE) {
                std::cerr << "Open file descriptors limit reached. Waiting for available spots."
                          << std::endl;
                std::this_thread::sleep_for(ServerOptions::accept_timeout);
                continue;
            }

            std::cerr << "Failed to accept(): " << strerror(errno) << std::endl;
            return false;
        }

        inet_ntop(their_address.ss_family,
                  locateInternetAddress(their_address),
                  address_buffer,
                  sizeof(address_buffer));
        onAccept(std::move(sock), address_buffer, getPort(their_address));
    }

    return true;
}

void HttpServer::onAccept(Socket sock,
                          const gsl::not_null<gsl::czstring> address,
                          const int port) const noexcept {
    Session s {std::move(sock), address, port};
    std::thread([s = std::move(s)]() mutable {
        s.Run();
    }).detach();
}

} //namespace nginxpp
