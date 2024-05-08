#pragma once

#include <string>
#include <ranges>
#include <vector>

namespace utils
{
    std::string generate_random_string(size_t length) noexcept;
    std::string& trim_left(std::string& str, std::string_view chars = "\t\n\v\f\r ") noexcept;
    std::string& trim_right(std::string& str, std::string_view chars = "\t\n\v\f\r ") noexcept;
    std::string& trim(std::string& str, std::string_view chars = "\t\n\v\f\r ") noexcept;
    std::vector<std::string> split(const std::string& str, std::string_view delim = " ") noexcept;
}