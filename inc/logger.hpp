#pragma once

#include <fstream>
#include <filesystem>
#include <format>
#include <mutex>
#include <cstdio>

constexpr std::string_view TERMINAL_COLOR_SUCCESS = "\033[32m";
constexpr std::string_view TERMINAL_COLOR_MESSAGE = "\033[34m";
constexpr std::string_view TERMINAL_COLOR_WARNING = "\033[33m";
constexpr std::string_view TERMINAL_COLOR_ERROR = "\033[31m";
constexpr std::string_view TERMINAL_COLOR_RESET = "\033[0m";

class logger final
{
public:
    enum class level : char
    {
        raw = -1,
        success = 0,
        message,
        warning,
        error,
        none
    };

    explicit logger(const std::filesystem::path path, level display_level = level::raw) noexcept;
    ~logger() noexcept;

    void set_display_level(level level)
    {
        error_display_level_ = level;
    }

    void log(const std::string& message, level log_level) noexcept;

    void newline() noexcept;

    void raw(const std::string& message) noexcept
    {
        log(message, level::raw);
    }

    void success(const std::string& message) noexcept
    {
        log(message, level::success);
    }

    void message(const std::string& message) noexcept
    {
        log(message, level::message);
    }

    void warning(const std::string& message) noexcept
    {
        log(message, level::warning);
    }

    void error(const std::string& message) noexcept
    {
        log(message, level::error);
    }

    bool is_open() const noexcept
    {
        return log_file_.is_open();
    }

    void flush() noexcept
    {
        log_file_.flush();
        fflush(stderr);
    }

    void close() noexcept
    {
        log_file_.close();
    }

    // format style log support
    template<typename... Args>
    void log(level log_level, const std::format_string<Args...>& format, Args&&... args) noexcept
    {
        log(std::format(format, std::forward<Args>(args)...), log_level);
    }

    template<typename... Args>
    void raw(const std::format_string<Args...>& format, Args&&... args) noexcept
    {
        log(level::raw, format, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void success(const std::format_string<Args...>& format, Args&&... args) noexcept
    {
        log(level::success, format, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void message(const std::format_string<Args...>& format, Args&&... args) noexcept
    {
        log(level::message, format, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void warning(const std::format_string<Args...>& format, Args&&... args) noexcept
    {
        log(level::warning, format, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void error(const std::format_string<Args...>& format, Args&&... args) noexcept
    {
        log(level::error, format, std::forward<Args>(args)...);
    }

private:
    std::ofstream log_file_;
    std::mutex log_mutex_;
    level error_display_level_;
};