#pragma once

#include <string_view>


namespace nginxpp {

[[nodiscard]]
std::string_view GetVersion() noexcept;

[[nodiscard]]
std::string_view GetGitDescribe() noexcept;

}//namespace nginxpp
