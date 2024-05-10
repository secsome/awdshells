#pragma once

#include "session.hpp"
#include "server.hpp"
#include "externs.hpp"

#include <unordered_set>
#include <memory>
#include <mutex>
#include <latch>
#include <filesystem>
#include <format>

class batch
{
public:
    using id_type = size_t;

    explicit batch(id_type id) noexcept
        : id_{ id }
    {
    }
    ~batch() = default;

    explicit batch(const batch&) = delete;
    batch& operator=(const batch&) = delete;

    explicit batch(batch&&) = delete;
    batch& operator=(batch&&) = delete;

    void add_session(session::id_type id) noexcept
    {
        std::lock_guard lock{ sessions_mutex_ };
        sessions_.insert(id);
    }

    void remove_session(session::id_type id) noexcept
    {
        std::lock_guard lock{ sessions_mutex_ };
        sessions_.erase(id);
    }

    void for_each_session(auto func, bool only_alive = true) noexcept
    {
        std::lock_guard lock{ sessions_mutex_ };
        auto alive_count = std::count_if(sessions_.begin(), sessions_.end(), [only_alive](const auto& id)
        {
            const auto manager = Manager->get_session(id);
            if (!manager)
                return false;
            return !only_alive || manager->is_alive();
        });
        std::latch waiter{ alive_count }; // FIXME: may underflow

        auto func_wrapper = [func, &waiter](std::shared_ptr<session> session) -> asio::awaitable<void>
        {
            co_await func(session);
            waiter.count_down();
        };

        for (const auto& id : sessions_)
        {
            const auto session = Manager->get_session(id);
            if (!session)
                continue;
            if (!only_alive || session->is_alive())
                asio::co_spawn(Manager->io_context(), func_wrapper(session), asio::detached);
        }

        waiter.wait();
    }
    asio::awaitable<void> async_for_each_session(auto func, bool only_alive = true) noexcept
    {
        for_each_session(func, only_alive);
        co_return;
    }

    void execute(std::string command) noexcept;
    void upload(std::filesystem::path filepath, std::string target_name) noexcept;
    void list() noexcept;

    void operate() noexcept;

    friend std::formatter<batch>;

private:
    id_type id_;
    std::unordered_set<session::id_type> sessions_;
    std::mutex sessions_mutex_;
};

template <>
struct std::formatter<batch> : std::formatter<std::string>
{
    template <typename FormatContext>
    auto format(const batch& s, FormatContext& ctx) const
    {
        // We need to const_cast here because we need to lock the mutex
        auto& srv = const_cast<batch&>(s);
        std::lock_guard lock{ srv.sessions_mutex_ };

        auto str = std::format("batch: id={}\tsession = ", s.id_);
        if (s.sessions_.empty())
            str += "[EMPTY]";
        else
        {
            for (const auto& session_id : s.sessions_)
                str += std::format("{} ", session_id);
            str.pop_back();
        }
        return std::ranges::copy(str, ctx.out()).out;
    }
};