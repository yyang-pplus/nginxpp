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
};

void AddServerOptions(cxxopts::Options &options) noexcept;

[[nodiscard]]
ServerOptions HandleServerOptions(const cxxopts::ParseResult &parsed_options) noexcept;


class HttpServer {
public:
    explicit HttpServer(ServerOptions options): m_options(std::move(options)) {
    }

private:
    ServerOptions m_options;
};

}//namespace nginxpp
