#include <nginxpp/server.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <iostream>
#include <streambuf>
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
#include <nginxpp/message.hpp>


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

[[nodiscard]] inline auto
read(const Socket &sock, char *const buffer, const std::size_t len) noexcept {
    Expects(buffer);
    return handleEINTR(recv, sock, buffer, len, 0);
}

[[nodiscard]] inline auto
write(const Socket &sock, const char *const buffer, const std::size_t len) noexcept {
    Expects(buffer);
    return handleEINTR(send, sock, buffer, len, 0);
}

[[nodiscard]] inline auto
sendAll(const Socket &sock, const char *const buffer, const std::size_t requested) noexcept {
    Expects(buffer);
    for (std::size_t total_sent = 0; total_sent < requested;) {
        const auto left = requested - total_sent;
        const auto n = write(sock, buffer + total_sent, left);
        if (n == -1) {
            return false;
        }
        total_sent += n;
    }

    return true;
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


class SocketBuf : public std::streambuf {
public:
    SocketBuf(Socket sock) noexcept : m_socket(std::move(sock)) {
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

        setg(m_in_buffer.begin(), m_in_buffer.begin(), m_in_buffer.begin());
        setp(m_out_buffer.begin(), m_out_buffer.begin() + SIZE - 1);
    }

    SocketBuf(const SocketBuf &) = delete;
    SocketBuf &operator=(const SocketBuf &) = delete;
    SocketBuf(SocketBuf &&) = default;
    SocketBuf &operator=(SocketBuf &&) = default;

    virtual ~SocketBuf() {
        sync();
    }

protected:
    static constexpr int SIZE = 4096;
    static constexpr int MAX_PUTBACK = 8;

    virtual int_type underflow() override {
        if (gptr() < egptr()) {
            return traits_type::to_int_type(*gptr());
        }

        const auto num_putback = std::min(MAX_PUTBACK, static_cast<int>(gptr() - eback()));
        std::copy(gptr() - num_putback, gptr(), m_in_buffer.begin());

        auto *const new_gptr = m_in_buffer.begin() + num_putback;
        const auto n = read(m_socket, new_gptr, SIZE - num_putback);
        if (n <= 0) {
            return traits_type::eof();
        }

        setg(m_in_buffer.begin(), new_gptr, new_gptr + n);

        return traits_type::to_int_type(*gptr());
    }

    auto flushBuffer() noexcept {
        if (pptr() == pbase()) {
            return true;
        }

        const auto requested = pptr() - pbase();
        const auto result = sendAll(m_socket, pbase(), requested);
        pbump(-requested);

        return result;
    }

    virtual int_type overflow(int_type c) override {
        if (not traits_type::eq_int_type(c, traits_type::eof())) {
            *pptr() = c;
            pbump(1);
        }

        return flushBuffer() ? traits_type::not_eof(c) : traits_type::eof();
    }

    virtual int sync() override {
        return flushBuffer() ? 0 : -1;
    }

private:
    std::array<char_type, SIZE> m_in_buffer;
    std::array<char_type, SIZE> m_out_buffer;
    pollfd m_poll_fd {};
    Socket m_socket;
};


class SocketStream : public std::iostream {
public:
    explicit SocketStream(Socket sock) noexcept : std::iostream(nullptr), m_buf(std::move(sock)) {
        rdbuf(&m_buf);
    }

    SocketStream(SocketStream &&rhs) noexcept :
        std::iostream(std::move(rhs)), m_buf(std::move(rhs.m_buf)) {
        rdbuf(&m_buf);
    }

    auto &operator=(SocketStream &&rhs) noexcept {
        if (this != &rhs) {
            std::iostream::operator=(std::move(rhs));
            m_buf = std::move(rhs.m_buf);
        }
        return *this;
    }

private:
    SocketBuf m_buf;
};


class Session {
public:
    Session(Socket sock,
            const gsl::not_null<gsl::czstring> address,
            const int port,
            std::filesystem::path root_dir) noexcept;

    void Run() noexcept;

private:
    [[nodiscard]] auto &log(std::ostream &out = std::cout) const noexcept {
        return out << '[' << m_id << "] ";
    }

    static unsigned session_created;

    SocketStream m_stream;
    std::filesystem::path m_root_dir;
    unsigned m_id {};
};

unsigned Session::session_created = 0;

Session::Session(Socket sock,
                 const gsl::not_null<gsl::czstring> address,
                 const int port,
                 std::filesystem::path root_dir) noexcept :
    m_stream(std::move(sock)),
    m_root_dir(std::move(root_dir)), m_id(session_created++) {
    log() << "Accepted new connection from: " << address << "; Port: " << port
          << "; Session: " << m_id << std::endl;
}

void Session::Run() noexcept {
    while (not g_signal) {
        const auto a_response = Handle(ParseOne(m_stream), m_root_dir);

        if (not a_response) {
            log() << a_response.error_str << std::endl;
        }
        m_stream << a_response << std::flush;
        break;
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
    m_root_dir(options.base_mount_dir), m_socket(internal::createServerSocket(options)),
    m_port(options.port) {
    Expects(m_socket != Socket::INVALID_SOCKET);

    if (not std::filesystem::exists(m_root_dir)) {
        throw ServerException {"Base mount directory doesn't exist: '" + options.base_mount_dir +
                               '\''};
    }
    m_root_dir = canonical(m_root_dir);

    if (m_port == 0) {
        m_port = internal::getPort(m_socket);
    }

    Ensures(m_port != 0);
}

void HttpServer::greet() const noexcept {
    std::cout << R"(
 _ __   __ _(_)
| '_ \ / _` | | '_ \\ \/ / '_ \| '_ \
| | | | (_| | | | | |>  <| |_) | |_) |
|_| |_|\__, |_|_| |_/_/\_\ .__/| .__/
       |___/             |_|   |_|      starting up.
)"
              << "Listening on port: " << m_port << '\n'
              << "Base mount directory: " << m_root_dir << std::endl;
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

        if (not hasConnectionRequest(server_pfd)) {
            continue;
        }

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
    Session s {std::move(sock), address, port, m_root_dir};
    std::thread([s = std::move(s)]() mutable {
        s.Run();
    }).detach();
}

} //namespace nginxpp
