#pragma once

#include <cxxopts.hpp>


namespace nginxpp {

[[nodiscard]] cxxopts::Options CreateBaseOptions() noexcept;

void HandleBaseOptions(const cxxopts::Options &options,
                       const cxxopts::ParseResult &parsed_options) noexcept;

} //namespace nginxpp
