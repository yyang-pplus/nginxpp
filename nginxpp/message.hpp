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
    void SetBody(std::string b) noexcept {
        headers["Content-Type"] = "text/html; charset=ascii";
        headers["Content-Length"] = std::to_string(b.size());

        body = std::move(b);
    }

    HeaderMap headers;
    std::string body;
    int status {};
};

[[nodiscard]] Request ParseOne(std::istream &in);

[[nodiscard]] Response Handle(const Request &a_request) noexcept;

std::ostream &operator<<(std::ostream &out, const Response &a_response) noexcept;

} //namespace nginxpp
