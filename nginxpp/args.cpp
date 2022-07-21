#include <nginxpp/args.hpp>
#include <nginxpp/version.hpp>


namespace nginxpp {

cxxopts::Options CreateBaseOptions() noexcept {
    cxxopts::Options options("nginxpp", "Yet another web server in C++.");

    options.add_options()
    ("h,help", "print usage")
    ("V,version", "print version")
    ;

    return options;
}

void HandleBaseOptions(const cxxopts::Options &options,
                       const cxxopts::ParseResult &parsed_options) noexcept {
    if (parsed_options.count("help")) {
        std::cout << options.help() << std::endl;
        exit(EXIT_SUCCESS);
    }

    if (parsed_options.count("version")) {
        std::cout << "Version: " << GetVersion() << std::endl;
        std::cout << "Git Description: " << GetGitDescribe() << std::endl;
        exit(EXIT_SUCCESS);
    }
}

}//namespace nginxpp
