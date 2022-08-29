#pragma once

#include <filesystem>
#include <iosfwd>
#include <string>
#include <unordered_map>


namespace nginxpp {

constexpr std::size_t MAX_LINE_LENGTH = 8192;
constexpr auto VERSION = "HTTP/1.1";


enum class Method { UNKNOWN, GET, HEAD, POST, PUT, DELETE, CONNECT, OPTIONS, TRACE, PATCH, PRI };

using HeaderMap = std::unordered_map<std::string, std::string>;

struct Message {
    HeaderMap headers;

    std::string error_str;
    int status = 200;

    operator bool() const noexcept {
        return error_str.empty();
    }
};

struct Request : public Message {
    std::string target;
    std::string version;
    Method method {};
};

struct Response : public Message {
    std::unique_ptr<std::iostream> body_stream;
};

[[nodiscard]] Request ParseOne(std::istream &in) noexcept;

[[nodiscard]] Response Handle(Request a_request, const std::filesystem::path &root_dir) noexcept;

std::ostream &operator<<(std::ostream &out, const Response &a_response) noexcept;

} //namespace nginxpp
