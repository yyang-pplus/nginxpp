#include <nginxpp/message.hpp>

#include <istream>
#include <string>

#include <iostream>

using namespace nginxpp;


namespace {} //namespace


namespace nginxpp {

[[nodiscard]] Request ParseOne(std::istream &in) noexcept {
    std::string a_line;
    while (std::getline(in, a_line)) {
        std::cout << a_line << std::endl;
    }

    return {};
}

} //namespace nginxpp
