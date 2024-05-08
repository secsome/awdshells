#pragma once

#include <asio.hpp>

#include <iostream>
#include <format>
#include <future>
#include <format>

class session final : public std::enable_shared_from_this<session>
{
public:
    using id_type = size_t;

    explicit session(asio::ip::tcp::socket&& socket, id_type id) noexcept
        : socket_{ std::move(socket) }
        , id_{ id }
    {
    }

    ~session() noexcept
    {
        if (socket_.is_open())
            socket_.close();
    }

    explicit session(const session&) = delete;
    session& operator=(const session&) = delete;

    explicit session(session&&) = delete;
    session& operator=(session&&) = delete;

    asio::ip::tcp::socket& socket() noexcept
    {
        return socket_;
    }

    const asio::ip::tcp::socket& socket() const noexcept
    {
        return socket_;
    }

    bool& is_alive() noexcept
    {
        return is_alive_;
    }

    const bool& is_alive() const noexcept
    {
        return is_alive_;
    }

    bool& is_echo() noexcept
    {
        return is_echo_;
    }

    const bool& is_echo() const noexcept
    {
        return is_echo_;
    }

    bool& is_timeout() noexcept
    {
        return is_timeout_;
    }

    const bool& is_timeout() const noexcept
    {
        return is_timeout_;
    }

    id_type& id() noexcept
    {
        return id_;
    }

    const id_type& id() const noexcept
    {
        return id_;
    }

    asio::ip::tcp::endpoint remote_endpoint() const noexcept
    {
        return socket_.remote_endpoint();
    }

    // async version
    asio::awaitable<std::string> async_read() noexcept;
    asio::awaitable<std::string> async_read(std::chrono::milliseconds timeout) noexcept;
    asio::awaitable<void> async_write(const std::string& data) noexcept;
    asio::awaitable<std::string> async_read_until(const std::string& delim) noexcept;
    asio::awaitable<std::string> async_read_until(const std::string& delim, std::chrono::milliseconds timeout) noexcept;
    asio::awaitable<std::string> execute_command(const std::string& command) noexcept;

    void interact(std::istream& is = std::cin, std::ostream& os = std::cout);

private:
    asio::ip::tcp::socket socket_;
    std::string read_buffer_;
    bool is_alive_{ true };
    bool is_echo_{ false };
    bool is_timeout_{ false };
    id_type id_{ 0 };
    std::mutex read_mutex_;
    std::mutex write_mutex_;
};

template <>
struct std::formatter<session> : std::formatter<std::string>
{
    template <typename FormatContext>
    auto format(const session& s, FormatContext& ctx) const
    {
        const auto str = std::format("session: id={}\thost: {}:{}\techo: {}",
            s.id(), s.remote_endpoint().address().to_string(),
            s.remote_endpoint().port(), s.is_echo());
        return std::ranges::copy(str, ctx.out()).out;
    }
};