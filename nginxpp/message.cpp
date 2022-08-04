#include <nginxpp/message.hpp>

#include <istream>
#include <regex>
#include <string>

#include <gsl/gsl>

#include <nginxpp/exception.hpp>
#include <nginxpp/string_utils.hpp>


using namespace nginxpp;


namespace {

[[nodiscard]] auto parseMethod(const std::string_view method_str) {

    static const std::unordered_map<std::string_view, Method> method_map {
        {"GET", Method::GET},
        {"HEAD", Method::HEAD},
        {"POST", Method::POST},
        {"PUT", Method::PUT},
        {"DELETE", Method::DELETE},
        {"CONNECT", Method::CONNECT},
        {"OPTIONS", Method::OPTIONS},
        {"TRACE", Method::TRACE},
        {"PATCH", Method::PATCH},
        {"PRI", Method::PRI}};

    const auto iter = method_map.find(method_str);
    if (iter == method_map.cend()) {
        throw ParserException {"Unknown Method: '" + std::string(method_str) + '\''};
    }

    return iter->second;
}

[[nodiscard]] auto decodeURI(std::string uri) noexcept {
    return uri;
}

[[nodiscard]] auto parseStartLine(std::istream &in) {
    std::string start_line;
    if (not std::getline(in, start_line)) {
        throw ParserException {"No start line"};
    }

    static const std::regex start_line_regex {R"(^(\S+)\s(\S+)\s(\S+)\s*$)"};

    std::smatch matches;
    if (not std::regex_match(start_line, matches, start_line_regex)) {
        throw ParserException {"Invalid start line: '" + start_line + '\''};
    }

    Request a_request;
    a_request.method = parseMethod(matches[1].str());
    a_request.target = decodeURI(matches[2].str());
    a_request.version = matches[3].str();

    if (a_request.version != "HTTP/1.1" and a_request.version != "HTTP/1.0") {
        throw HttpVersionException {"HTTP version '" + a_request.version + "' is not supported"};
    }

    return a_request;
}

void parseOneHeader(const std::string &a_header,
                    std::unordered_map<std::string, std::string> &headers) noexcept {
    static const std::regex header_regex {R"(^\s*(\S+)\s*:\s*(.+)\s*$)"};

    std::smatch matches;
    if (std::regex_match(a_header, matches, header_regex)) {
        headers.emplace(ToLower(matches[1].str()), decodeURI(matches[2].str()));
    }
}

[[nodiscard]] auto parseHeaders(std::istream &in) {
    std::unordered_map<std::string, std::string> headers;

    std::string a_line;
    while (std::getline(in, a_line)) {
        if (a_line.empty()) {
            break;
        }
        parseOneHeader(a_line, headers);
    }
    in.clear();

    Ensures(in);
    return headers;
}

[[nodiscard]] auto toStatusText(const int code) {
    switch (code) {
    case 100:
        return "Continue";
    case 101:
        return "Switching Protocol";
    case 102:
        return "Processing";
    case 103:
        return "Early Hints";
    case 200:
        return "OK";
    case 201:
        return "Created";
    case 202:
        return "Accepted";
    case 203:
        return "Non-Authoritative Information";
    case 204:
        return "No Content";
    case 205:
        return "Reset Content";
    case 206:
        return "Partial Content";
    case 207:
        return "Multi-Status";
    case 208:
        return "Already Reported";
    case 226:
        return "IM Used";
    case 300:
        return "Multiple Choice";
    case 301:
        return "Moved Permanently";
    case 302:
        return "Found";
    case 303:
        return "See Other";
    case 304:
        return "Not Modified";
    case 305:
        return "Use Proxy";
    case 306:
        return "unused";
    case 307:
        return "Temporary Redirect";
    case 308:
        return "Permanent Redirect";
    case 400:
        return "Bad Request";
    case 401:
        return "Unauthorized";
    case 402:
        return "Payment Required";
    case 403:
        return "Forbidden";
    case 404:
        return "Not Found";
    case 405:
        return "Method Not Allowed";
    case 406:
        return "Not Acceptable";
    case 407:
        return "Proxy Authentication Required";
    case 408:
        return "Request Timeout";
    case 409:
        return "Conflict";
    case 410:
        return "Gone";
    case 411:
        return "Length Required";
    case 412:
        return "Precondition Failed";
    case 413:
        return "Payload Too Large";
    case 414:
        return "URI Too Long";
    case 415:
        return "Unsupported Media Type";
    case 416:
        return "Range Not Satisfiable";
    case 417:
        return "Expectation Failed";
    case 418:
        return "I'm a teapot";
    case 421:
        return "Misdirected Request";
    case 422:
        return "Unprocessable Entity";
    case 423:
        return "Locked";
    case 424:
        return "Failed Dependency";
    case 425:
        return "Too Early";
    case 426:
        return "Upgrade Required";
    case 428:
        return "Precondition Required";
    case 429:
        return "Too Many Requests";
    case 431:
        return "Request Header Fields Too Large";
    case 451:
        return "Unavailable For Legal Reasons";
    case 501:
        return "Not Implemented";
    case 502:
        return "Bad Gateway";
    case 503:
        return "Service Unavailable";
    case 504:
        return "Gateway Timeout";
    case 505:
        return "HTTP Version Not Supported";
    case 506:
        return "Variant Also Negotiates";
    case 507:
        return "Insufficient Storage";
    case 508:
        return "Loop Detected";
    case 510:
        return "Not Extended";
    case 511:
        return "Network Authentication Required";

    default:
    case 500:
        return "Internal Server Error";
    }
}

} //namespace


namespace nginxpp {

[[nodiscard]] Request ParseOne(std::istream &in) {
    auto a_request = parseStartLine(in);
    a_request.headers = parseHeaders(in);

    return a_request;
}

[[nodiscard]] Response Handle(const Request &) noexcept {
    Response a_response;
    a_response.status = 200;

    a_response.SetBody("Hello World");

    return a_response;
}

std::ostream &operator<<(std::ostream &out, const Response &a_response) noexcept {
    out << VERSION << ' ' << a_response.status << ' ' << toStatusText(a_response.status) << '\n';

    for (const auto &[key, value] : a_response.headers) {
        out << key << ": " << value << '\n';
    }

    return out << '\n' << a_response.body;
}

} //namespace nginxpp
