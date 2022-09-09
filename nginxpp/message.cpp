#include <nginxpp/message.hpp>

#include <fstream>
#include <istream>
#include <regex>
#include <sstream>
#include <string>

#include <gsl/gsl>

#include <nginxpp/chrono_utils.hpp>
#include <nginxpp/exception.hpp>
#include <nginxpp/path_utils.hpp>
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

[[nodiscard]] auto parseStartLine(std::istream &in) noexcept {
    Request a_request;

    std::string start_line;
    if (not std::getline(in, start_line)) {
        a_request.status = 400;
        a_request.error_str = "No start line";
        return a_request;
    }

    static const std::regex start_line_regex {R"(^(\S+)\s(\S+)\s(\S+)\s*$)"};

    std::smatch matches;
    if (not std::regex_match(start_line, matches, start_line_regex)) {
        a_request.status = 400;
        a_request.error_str = "Invalid start line: '" + start_line + '\'';
        return a_request;
    }

    const auto method_str = matches[1].str();
    try {
        a_request.method = parseMethod(method_str);
    } catch (const ParserException &e) {
        a_request.status = 400;
        a_request.error_str = e.what();
        return a_request;
    }
    a_request.target = decodeURI(matches[2].str());
    if (not a_request.target.empty() and a_request.target.front() == '/') {
        a_request.target.erase(a_request.target.cbegin());
    }
    a_request.version = matches[3].str();

    if (a_request.version != "HTTP/1.1" and a_request.version != "HTTP/1.0") {
        a_request.status = 505;
        a_request.error_str = "HTTP version '" + a_request.version + "' is not supported";
        return a_request;
    }

    if (a_request.method != Method::GET and a_request.method != Method::HEAD) {
        a_request.status = 501;
        a_request.error_str = "HTTP method " + method_str + " not implemented";
        return a_request;
    }

    if (a_request.target.size() > MAX_LINE_LENGTH) {
        a_request.status = 414;
        a_request.error_str = "Target URI length " + std::to_string(a_request.target.size()) +
                              " exceeds maximum " + std::to_string(MAX_LINE_LENGTH);
        return a_request;
    }

    return a_request;
}

[[nodiscard]] auto parseOneHeader(const std::string &a_header, Request &a_request) noexcept {
    static const std::regex header_regex {R"(^\s*(\S+)\s*:\s*(.+)\s*$)"};

    if (a_header.empty()) {
        return false;
    }

    std::smatch matches;
    if (std::regex_match(a_header, matches, header_regex)) {
        auto value = decodeURI(matches[2].str());
        const auto [iter, inserted] =
            a_request.headers.try_emplace(ToLower(matches[1].str()), std::move(value));
        if (not inserted) {
            iter->second += ", " + std::move(value);
        }

        if (iter->second.size() > MAX_LINE_LENGTH) {
            a_request.status = 431;
            a_request.error_str = "Header field '" + iter->first + "' length " +
                                  std::to_string(iter->second.size()) + " exceeds maximum " +
                                  std::to_string(MAX_LINE_LENGTH);
            return false;
        }
    }

    return true;
}

void parseHeaders(std::istream &in, Request &a_request) noexcept {
    std::string a_line;
    while (std::getline(in, a_line) and parseOneHeader(a_line, a_request)) {
    }
}

[[nodiscard]] auto toStatusText(const int code) noexcept {
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

[[nodiscard]] std::string toContentType(const std::filesystem::path &p) noexcept {
    static const std::unordered_map<std::string_view, std::string> CONTENT_TYPE_MAP = {
        {"css", "text/css"},
        {"csv", "text/csv"},
        {"htm", "text/html"},
        {"html", "text/html"},
        {"js", "text/javascript"},
        {"mjs", "text/javascript"},
        {"txt", "text/plain"},
        {"vtt", "text/vtt"},

        {"apng", "image/apng"},
        {"avif", "image/avif"},
        {"bmp", "image/bmp"},
        {"gif", "image/gif"},
        {"png", "image/png"},
        {"svg", "image/svg+xml"},
        {"webp", "image/webp"},
        {"ico", "image/x-icon"},
        {"tif", "image/tiff"},
        {"tiff", "image/tiff"},
        {"jpg", "image/jpeg"},
        {"jpeg", "image/jpeg"},

        {"mp4", "video/mp4"},
        {"mpeg", "video/mpeg"},
        {"webm", "video/webm"},

        {"mp3", "audio/mp3"},
        {"mpga", "audio/mpeg"},
        {"weba", "audio/webm"},
        {"wav", "audio/wave"},

        {"otf", "font/otf"},
        {"ttf", "font/ttf"},
        {"woff", "font/woff"},
        {"woff2", "font/woff2"},

        {"7z", "application/x-7z-compressed"},
        {"atom", "application/atom+xml"},
        {"pdf", "application/pdf"},
        {"json", "application/json"},
        {"rss", "application/rss+xml"},
        {"tar", "application/x-tar"},
        {"xht", "application/xhtml+xml"},
        {"xhtml", "application/xhtml+xml"},
        {"xslt", "application/xslt+xml"},
        {"xml", "application/xml"},
        {"gz", "application/gzip"},
        {"zip", "application/zip"},
        {"wasm", "application/wasm"},
    };

    const auto extension = ToLower(p.extension());
    const auto key = [](const std::string_view ext) {
        if (not ext.empty() and ext.front() == '.') {
            return ext.substr(1);
        }
        return ext;
    }(extension);

    if (const auto iter = CONTENT_TYPE_MAP.find(key); iter != CONTENT_TYPE_MAP.cend()) {
        return iter->second;
    }

    return "application/octet-stream";
}

inline auto toHtmlTableHeaderRow(const std::vector<std::string_view> &headers) noexcept {
    std::ostringstream oss;
    oss << "<tr>";
    for (const auto a_header : headers) {
        oss << "<th>" << a_header << "</th>";
    }
    oss << "</tr>";

    return oss.str();
}

template<typename T>
inline auto toHtmlTableCell(T &&v) noexcept {
    std::ostringstream oss;
    oss << "<td>" << v << "</td>";

    return oss.str();
}

inline auto toHtmlLink(const std::string_view relative_link,
                       const std::string_view text) noexcept {
    Expects(not(StartsWith(relative_link, "http://") or StartsWith(relative_link, "https://")));

    std::ostringstream oss;
    oss << "<a href=/" << relative_link << '>' << text << "</a>";

    return oss.str();
}

inline auto toHtmlTableRow(const PathStats &s, const std::filesystem::path &base) noexcept {
    std::ostringstream oss;
    oss << "<tr>"
        << toHtmlTableCell(toHtmlLink(std::filesystem::relative(s.path, base).string(),
                                      s.path.filename().string()))
        << toHtmlTableCell(s.modification_time)
        << toHtmlTableCell(s.size == -1 ? "" : std::to_string(s.size)) << "</tr>";

    return oss.str();
}

constexpr auto *HTML_STYLE = R"(
<style>
table {
    font-family: arial, sans-serif;
    border-collapse: collapse;
    width: 100%;
}

td, th {
    border: 1px solid #dddddd;
    text-align: left;
    padding: 8px;
}

tr:nth-child(even) {
    background-color: #dddddd;
}
</style>)";

auto &buildLsTable(std::ostream &out,
                   const std::filesystem::path &p,
                   const std::filesystem::path &root_dir) noexcept {
    Expects(is_directory(p));

    static const std::vector<std::string_view> ls_headers = {"Name", "Date Modified", "Size"};
    const auto children = GetChildStats(p);

    out << "<table>";
    out << toHtmlTableHeaderRow(ls_headers);
    out << toHtmlTableRow(PathStats {p / "."}, root_dir);
    if (p != root_dir) {
        out << toHtmlTableRow(PathStats {p / ".."}, root_dir);
    }
    for (const auto &s : children) {
        out << toHtmlTableRow(s, root_dir);
    }

    return out << "</table>";
}

auto &buildLsHeader(std::ostream &out,
                    const std::filesystem::path &p,
                    const std::filesystem::path &root_dir) noexcept {
    const auto relative_path = std::filesystem::relative(p, root_dir);
    std::filesystem::path prefix;
    out << "<h1>";
    out << toHtmlLink(prefix.string(), "/");
    if (p != root_dir) {
        for (auto iter = relative_path.begin(); iter != relative_path.end(); ++iter) {
            prefix /= *iter;
            out << toHtmlLink(prefix.string(), ' ' + iter->string() + " /");
        }
    }
    return out << "</h1>";
}

auto &buildLsPage(std::ostream &out,
                  const std::filesystem::path &p,
                  const std::filesystem::path &root_dir) noexcept {
    Expects(StartsWith(p, root_dir));

    out << "<!DOCTYPE html>";
    out << "<html>";
    out << "<head>";
    out << HTML_STYLE;
    out << "</head>";

    out << "<body>";
    buildLsHeader(out, p, root_dir);
    buildLsTable(out, p, root_dir);
    out << "</body>";
    return out << "</html>";
}

} //namespace


namespace nginxpp {

[[nodiscard]] Request ParseOne(std::istream &in) noexcept {
    const auto in_final = gsl::finally([&in]() {
        in.clear();
    });

    auto a_request = parseStartLine(in);
    if (a_request) {
        parseHeaders(in, a_request);
    }

    return a_request;
}

[[nodiscard]] Response Handle(Request a_request, const std::filesystem::path &root_dir) noexcept {
    Response a_response;
    a_response.status = a_request.status;
    a_response.error_str = std::move(a_request.error_str);
    if (not a_response) {
        return a_response;
    }

    const auto p = weakly_canonical(root_dir / a_request.target);

    if (not StartsWith(p, root_dir)) {
        a_response.status = 403;
        a_response.error_str = "Access to '" + p.string() + "' not allowed";
        return a_response;
    }

    if (not std::filesystem::exists(p)) {
        a_response.status = 404;
        a_response.error_str = "Target '" + p.string() + "' not found";
        return a_response;
    }

    if (is_directory(p)) {
        auto ss = std::make_unique<std::stringstream>();
        buildLsPage(*ss, p, root_dir);

        a_response.headers["Content-Type"] = "text/html; charset=ascii";
        a_response.headers["Content-Length"] = std::to_string(ss->str().size());
        a_response.body_stream = std::move(ss);

    } else if (is_regular_file(p)) {
        PathStats stats {p};
        a_response.headers["Content-Type"] = toContentType(p);
        a_response.headers["Content-Length"] = std::to_string(stats.size);
        a_response.body_stream = std::make_unique<std::fstream>(p, std::ios::binary);

    } else {
        a_response.status = 500;
        a_response.error_str = "File type '" + p.string() + "' not supported";
        return a_response;
    }

    return a_response;
}

std::ostream &operator<<(std::ostream &out, const Response &a_response) noexcept {
    out << VERSION << ' ' << a_response.status << ' ' << toStatusText(a_response.status) << '\n';

    for (const auto &[key, value] : a_response.headers) {
        out << key << ": " << value << '\n';
    }
    out << '\n';

    if (a_response.body_stream) {
        out << a_response.body_stream->rdbuf();
    }

    return out;
}

} //namespace nginxpp
