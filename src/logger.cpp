#include "logger.hpp"

#include <print>
#include <chrono>
#include <utility>

logger::logger(const std::filesystem::path path, level display_level) noexcept
    : error_display_level_ { display_level }
{
    log_file_.open(path, std::ios::out | std::ios::trunc | std::ios::binary);
    if (!log_file_.is_open())
        std::print(stderr, "Failed to open log file: {}\n", path.string());
}

logger::~logger() noexcept
{
    log_file_.close();
}

void logger::log(const std::string& message, level log_level) noexcept
{
    if (log_file_.is_open())
    {
        std::lock_guard<std::mutex> lock(log_mutex_);
        const auto current_time = std::chrono::system_clock::now();
        std::string current_time_string = std::format("[{:%Y-%m-%d %H:%M:%S}]", current_time);

        std::string final;
        switch (log_level)
        {
        case level::raw:
            final = TERMINAL_COLOR_RESET;
            final += current_time_string;
            break;
        case level::success:
            final = TERMINAL_COLOR_SUCCESS;
            final += current_time_string;
            final += "[SUCCESS]";
            break;
        case level::message:
            final = TERMINAL_COLOR_MESSAGE;
            final += current_time_string;
            final += "[MESSAGE]";
            break;
        case level::warning:
            final = TERMINAL_COLOR_WARNING;
            final += current_time_string;
            final += "[WARNING]";
            break;
        case level::error:
            final = TERMINAL_COLOR_ERROR;
            final += current_time_string;
            final += "[ERROR]";
            break;
        default:
            std::unreachable();
            break;
        }

        final += " ";
        final += message;
        final += TERMINAL_COLOR_RESET;
        if (log_level >= error_display_level_)
            std::print(stderr, "{}", final);

        log_file_ << final;
    }
}

void logger::newline() noexcept
{
    if (log_file_.is_open())
    {
        std::lock_guard<std::mutex> lock(log_mutex_);
        std::print(stderr, "\n");
        log_file_ << "\n";
    }
}
