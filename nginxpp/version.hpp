#pragma once

#include <string_view>


namespace nginxpp {

[[nodiscard]]
std::string_view GetVersion();

[[nodiscard]]
std::string_view GetGitDescribe();

}//namespace nginxpp
