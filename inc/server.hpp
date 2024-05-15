#pragma once

#include <asio.hpp>

#include <string>
#include <map>
#include <mutex>
#include <numeric>
#include <atomic>
#include <latch>
#include <format>

#include "session.hpp"

class server final
{
public:
    // constructor: initialize with address and port
    explicit server(const std::string& address,
        asio::ip::port_type port,
        int concurrency_hint,
        size_t max_session_count = std::numeric_limits<size_t>::max()
    ) noexcept;

    void start() noexcept;
    void stop() noexcept;

    void remove_session(session::id_type id) noexcept;

    void set_alive_checker_status(bool enable) noexcept;

    void for_each_session(auto func, bool only_alive = true) noexcept
    {
        std::lock_guard lock{ sessions_mutex_ };
        auto alive_count = std::count_if(sessions_.begin(), sessions_.end(), [only_alive](const auto& itr)
        {
            return (!only_alive || itr.second->is_alive());
        });
        std::latch waiter{ alive_count }; // FIXME: may underflow

        auto func_wrapper = [func, &waiter](std::shared_ptr<session> session) -> asio::awaitable<void>
        {
            co_await func(session);
            waiter.count_down();
        };

        for (const auto& [id, session] : sessions_)
        {
            if (!only_alive || session->is_alive())
                asio::co_spawn(io_context_, func_wrapper(session), asio::detached);
        }

        waiter.wait();
    }
    asio::awaitable<void> async_for_each_session(auto func, bool only_alive = true) noexcept
    {
        std::lock_guard lock{ sessions_mutex_ };
        auto alive_count = std::count_if(sessions_.begin(), sessions_.end(), [only_alive](const auto& itr)
        {
            return !only_alive || itr.second->is_alive();
        });
        std::latch waiter{ alive_count }; // FIXME: may underflow

        auto func_wrapper = [func, &waiter](std::shared_ptr<session> session) -> asio::awaitable<void>
        {
            co_await func(session);
            waiter.count_down();
        };

        for (const auto& [id, session] : sessions_)
        {
            if (!only_alive || session->is_alive())
                asio::co_spawn(io_context_, func_wrapper(session), asio::detached);
        }

        waiter.wait();
        co_return;
    }

    std::shared_ptr<session> get_session(session::id_type id) noexcept
    {
        std::lock_guard lock{ sessions_mutex_ };
        if (auto it = sessions_.find(id); it != sessions_.end())
            return it->second;
        return nullptr;
    }

    size_t session_count() const noexcept
    {
        auto& srv = const_cast<server&>(*this);
        std::lock_guard lock{ srv.sessions_mutex_ };
        return sessions_.size();
    }

    auto& io_context() noexcept
    {
        return io_context_;
    }

    const auto& io_context() const noexcept
    {
        return io_context_;
    }

    friend std::formatter<server>;

private:
    asio::awaitable<void> accept_connection() noexcept;
    void alive_checker() noexcept;

    asio::io_context io_context_;
    asio::ip::tcp::endpoint endpoint_;
    asio::ip::tcp::acceptor acceptor_;
    std::map<session::id_type, std::shared_ptr<session>> sessions_;
    std::mutex sessions_mutex_;
    size_t max_session_count_;
    std::atomic_size_t current_session_id_{ 0 };

    std::atomic_bool is_running_{ false };
    std::atomic_bool skip_alive_check_{ false };
};

template <>
struct std::formatter<server>
{
    bool only_alive = false;

    template<typename ParseContext>
    constexpr auto parse(ParseContext& ctx)
    {
        auto it = ctx.begin();
        if (it == ctx.end())
            return it;

        if (*it == 'a')
        {
            only_alive = true;
            ++it;
        }
        if (it != ctx.end() && *it != '}')
            throw std::format_error("Invalid format args for server.");

        return it;
    }

    template <typename FormatContext>
    auto format(const server& s, FormatContext& ctx) const
    {
        // We need to const_cast here because we need to lock the mutex
        auto& srv = const_cast<server&>(s);
        std::lock_guard lock{ srv.sessions_mutex_ };

        if (s.sessions_.empty())
            return std::ranges::copy("[-]No session established\n", ctx.out()).out;

        std::string str;
        for (const auto& [id, session] : s.sessions_)
        {
            if (only_alive && !session->is_alive())
                continue;
            str += std::format("{}\n", *session);
        }

        return std::ranges::copy(str, ctx.out()).out;
    }
};