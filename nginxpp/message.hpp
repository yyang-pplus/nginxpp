#pragma once

#include <iosfwd>


namespace nginxpp {

struct Request {};

struct Response {};

[[nodiscard]] Request ParseOne(std::istream &in) noexcept;

} //namespace nginxpp
