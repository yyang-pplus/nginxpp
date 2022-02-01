#pragma once

#include <cxxopts.hpp>


namespace psqlxx {

[[nodiscard]]
cxxopts::Options CreateBaseOptions();

[[nodiscard]]
std::optional<cxxopts::ParseResult>
ParseOptions(cxxopts::Options &options, int argc, const char *argv[]) noexcept;

void HandleBaseOptions(const cxxopts::Options &options,
                       const cxxopts::ParseResult &parsed_options);

}//namespace psqlxx
