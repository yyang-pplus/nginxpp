#include <nginxpp/server.hpp>

#include <cxxopts.hpp>


using namespace nginxpp;


namespace nginxpp {

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

}//namespace nginxpp
