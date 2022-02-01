#pragma once

#include <string_view>


namespace psqlxx {

[[nodiscard]]
std::string_view GetVersion();

[[nodiscard]]
std::string_view GetGitDescribe();

}//namespace psqlxx
