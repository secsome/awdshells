#include "utils.hpp"

#include <string>
#include <random>
#include <vector>
#include <cctype>
#include <algorithm>

namespace utils {

std::string generate_random_string(size_t length) noexcept
{
    std::string result;
    result.reserve(length);

    std::random_device rd;
    std::mt19937 gen{ rd() };

    auto get_random_char = [&gen]() -> char
    {
        constexpr std::string_view charset = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
        std::uniform_int_distribution<size_t> dis(0, charset.length() - 1);
        return charset[dis(gen)];
    };

    std::generate_n(std::back_inserter(result), length, get_random_char);

    return result;
}

std::string& trim_left(std::string& str, std::string_view chars) noexcept
{
    str.erase(0, str.find_first_not_of(chars));
    return str;
}

std::string& trim_right(std::string& str, std::string_view chars) noexcept
{
    str.erase(str.find_last_not_of(chars) + 1);
    return str;
}

std::string& trim(std::string& str, std::string_view chars) noexcept
{
    trim_left(str, chars);
    trim_right(str, chars);
    return str;
}

std::vector<std::string> split(const std::string& str, std::string_view delim) noexcept
{
    std::vector<std::string> result;
    for (auto&& token : str | std::views::split(delim))
        result.emplace_back(token.begin(), token.end());
    return result;
}

std::string& colorize(std::string& str, string_color clr) noexcept
{
    constexpr std::string_view colors[] = {
        "\033[30m", "\033[31m", "\033[32m", "\033[33m",
        "\033[34m", "\033[35m", "\033[36m", "\033[37m",
        "\033[0m"
    };

    str.insert(0, colors[static_cast<uint8_t>(clr)]);
    str.append(colors[static_cast<uint8_t>(string_color::reset)]);
    return str;
}

} // namespace utils