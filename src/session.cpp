#include "session.hpp"

#include "externs.hpp"
#include "utils.hpp"

#include <semaphore>

asio::awaitable<std::string> session::async_read() noexcept
{
    try
    {
        std::lock_guard lock{ read_mutex_ };
        asio::streambuf buffer;
        co_await asio::async_read_until(socket_, buffer, "\0", asio::use_awaitable);
        is_alive_ = true;
        co_return std::string{ asio::buffers_begin(buffer.data()), asio::buffers_end(buffer.data()) };
    }
    catch (const std::exception& e)
    {
        is_alive_ = false;
        Log->error("Error reading until delimiter: {}\n", e.what());
        co_return "";
    }
}

asio::awaitable<std::string> session::async_read(std::chrono::milliseconds timeout) noexcept
{
    try
    {
        is_timeout_ = false;
        std::lock_guard lock{ read_mutex_ };
        std::string buffer;

        asio::steady_timer timer{ socket_.get_executor(), timeout };
        timer.expires_after(std::chrono::seconds(5));

        timer.async_wait([this](const asio::error_code& error)
        {
            if (!error) // timer was not cancelled
                socket_.cancel();
        });

        asio::error_code ec;
        co_await asio::async_read(socket_, asio::dynamic_buffer(buffer), asio::transfer_all(), asio::redirect_error(asio::use_awaitable, ec));

        timer.cancel();

        if (ec)
        {
            if (ec == asio::error::operation_aborted)
            {
                is_timeout_ = true;
                co_return buffer;
            }
            else
                Log->error("Error reading data: {}\n", ec.message());

            is_alive_ = false;
            co_return buffer;
        }

        is_alive_ = true;
        co_return buffer;
    }
    catch (const std::exception& e)
    {
        is_alive_ = false;
        Log->error("Error reading data: {}\n", e.what());
        co_return "";
    }
}

asio::awaitable<void> session::async_write(const std::string& data) noexcept
{
    try
    {
        std::lock_guard lock{ write_mutex_ };
        const std::string data_copy = data;
        co_await asio::async_write(socket_, asio::buffer(data_copy), asio::use_awaitable);
        is_alive_ = true;
    }
    catch (const std::exception& e)
    {
        Log->error("Error writing data: {}\n", e.what());
        is_alive_ = false;
    }
}

asio::awaitable<std::string> session::async_read_until(const std::string& delim) noexcept
{
    try
    {
        const std::string delim_copy = delim;
        std::lock_guard lock{ read_mutex_ };
        // Try to find delimiter in the buffer
        {
            const auto off = read_buffer_.find(delim_copy);
            if (off != std::string::npos)
            {
                auto&& result = read_buffer_.substr(0, off + delim_copy.size());
                read_buffer_ = read_buffer_.substr(off + delim_copy.size());
                co_return result;
            }
        }

        // Cannot found delimiter in the buffer, try to read from the connection
        std::string buffer;
        size_t length = co_await asio::async_read_until(socket_, asio::dynamic_buffer(buffer), delim, asio::use_awaitable);
        is_alive_ = true;
        std::string&& result = read_buffer_ + buffer.substr(0, length);
        read_buffer_ = buffer.substr(length);
        co_return result;
    }
    catch (const std::exception& e)
    {
        is_alive_ = false;
        Log->error("Error reading until delimiter: {}\n", e.what());
        co_return "";
    }
}

asio::awaitable<std::string> session::async_read_until(const std::string& delim, std::chrono::milliseconds timeout) noexcept
{
    try
    {
        const std::string delim_copy = delim;
        is_timeout_ = false;
        std::lock_guard lock{ read_mutex_ };

        // Try to find delimiter in the buffer
        {
            const auto off = read_buffer_.find(delim_copy);
            if (off != std::string::npos)
            {
                auto&& result = read_buffer_.substr(0, off + delim_copy.size());
                read_buffer_ = read_buffer_.substr(off + delim_copy.size());
                co_return result;
            }
        }

        // Cannot found delimiter in the buffer, try to read from the connection
        asio::steady_timer timer{ socket_.get_executor(), timeout };
        timer.async_wait([this](const asio::error_code& error)
        {
            if (!error) // timer was not cancelled
                socket_.cancel();
        });

        std::string buffer;
        asio::error_code ec;
        size_t length = co_await asio::async_read_until(socket_, asio::dynamic_buffer(buffer), delim_copy, asio::redirect_error(asio::use_awaitable, ec));

        timer.cancel();

        if (ec)
        {
            if (ec == asio::error::operation_aborted)
            {
                is_timeout_ = true;
                read_buffer_.append(buffer);
                co_return "";
            }
            else
                Log->error("Error reading until delimiter: {}\n", ec.message());

            is_alive_ = false;
            co_return "";
        }

        is_alive_ = true;
        std::string&& result = read_buffer_ + buffer.substr(0, length);
        read_buffer_ = buffer.substr(length);
        co_return result;
    }
    catch (const std::exception& e)
    {
        is_alive_ = false;
        Log->error("Error reading data: {}\n", e.what());
        co_return "";
    }
}

asio::awaitable<std::string> session::execute_command(const std::string& command) noexcept
{
    const auto prefix = utils::generate_random_string(8);
    const auto suffix = utils::generate_random_string(8);
    co_await async_write("echo " + prefix + " && " + command + "; echo " + suffix + "\n");
    if (!is_alive_)
        co_return "";

    auto&& response = co_await async_read_until(prefix);
    if (!is_alive_)
        co_return "";
    if (is_echo_)
    {
        response = co_await async_read_until(prefix);
        if (!is_alive_)
            co_return "";
    }
    response = co_await async_read_until(suffix);
    if (!is_alive_)
        co_return "";
    response.erase(response.size() - suffix.size());
    co_return utils::trim_left(response, "\r\n");
}

void session::interact(std::istream& is, std::ostream& os)
{
    std::binary_semaphore sem{ 0 };
    auto impl = [this, &is, &os, &sem]() -> asio::awaitable<void>
    {
        for (;;)
        {
            // get current directory
            auto&& pwd = co_await execute_command("pwd");
            if (!is_alive_)
                break;

            os << utils::trim_right(pwd, "\r\n") << "> ";
            std::string command;
            std::getline(is, command);
            if (command.empty())
                continue;

            if (command == "exit")
                break;

            const auto result = co_await execute_command(command);
            if (!is_alive_)
                break;

            os << result << std::endl;
        }
        sem.release();
    };

    asio::co_spawn(socket_.get_executor(), impl(), asio::detached);
    sem.acquire();
}
