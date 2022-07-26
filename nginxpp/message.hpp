#pragma once

#include <iosfwd>
#include <string>
#include <unordered_map>


namespace nginxpp {

constexpr int MAX_LINE_LENGTH = 8192;
constexpr auto VERSION = "HTTP/1.1";


enum class Method { UNKNOWN, GET, HEAD, POST, PUT, DELETE, CONNECT, OPTIONS, TRACE, PATCH, PRI };

using HeaderMap = std::unordered_map<std::string, std::string>;

struct Request {
    HeaderMap headers;
    std::string target;
    std::string version;
    Method method {};
};

struct Response {
    HeaderMap headers;
    int status {};
};

[[nodiscard]] Request ParseOne(std::istream &in);

std::ostream &operator<<(std::ostream &out, const Response &a_response) noexcept;

} //namespace nginxpp
