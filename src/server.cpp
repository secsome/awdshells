#include "server.hpp"

#include <vector>

#include "externs.hpp"
#include "utils.hpp"

server::server(
    const std::string& address,
    asio::ip::port_type port,
    int concurrency_hint,
    size_t max_session_count
) noexcept
    : io_context_{ concurrency_hint }
    , endpoint_{ asio::ip::make_address(address), port }
    , acceptor_ { io_context_, endpoint_ }
    , max_session_count_{ max_session_count }
{
}

void server::start() noexcept
{
    is_running_ = true;

    asio::co_spawn(io_context_, accept_connection(), asio::detached);

    std::thread{ [this] { alive_checker(); } }.detach();

    io_context_.run();
}

void server::stop() noexcept
{
    is_running_ = false;
    io_context_.stop();
    std::lock_guard lock{ sessions_mutex_ };
    for (auto& [id, session] : sessions_)
    {
        Log->message("Removing session {}\n", *session);
        try
        {
            session->socket().close();
        }
        catch (const std::exception& e)
        {
            Log->error("Error closing socket: {}\n", e.what());
        }
    }
    sessions_.clear();
}

void server::remove_session(session::id_type id) noexcept
{
    std::lock_guard lock{ sessions_mutex_ };
    if (auto it = sessions_.find(id); it != sessions_.end())
    {
        try
        {
            Log->success("Removing session {}\n", *it->second);
            it->second->socket().close();
        }
        catch (const std::exception& e)
        {
            Log->error("Error closing socket: {}\n", e.what());
        }
        sessions_.erase(it);
    }
    else
        Log->error("Session {} not found\n", id);
}

asio::awaitable<void> server::accept_connection() noexcept
{
    auto executor = co_await asio::this_coro::executor;

    auto accept_impl = [this](asio::ip::tcp::socket socket) -> asio::awaitable<void>
    {
        try
        {
            const auto current_session_id = current_session_id_++;

            auto new_session = std::make_shared<session>(std::move(socket), current_session_id);
            
            const auto echo_token = utils::generate_random_string(16);
            co_await new_session->async_write("echo " + echo_token + "\n");
            if (!new_session->is_alive())
                co_return;
            auto&& response1 = co_await new_session->async_read_until(echo_token);
            if (!new_session->is_alive())
                co_return;

            auto&& response2 = co_await new_session->async_read_until(echo_token, std::chrono::milliseconds{ 1000 });
            if (!new_session->is_timeout())
                new_session->is_echo() = true;

            std::lock_guard lock{ sessions_mutex_ };
            // check if we can accept more sessions
            if (sessions_.size() < max_session_count_)
            {
                sessions_.emplace(current_session_id, new_session);
                Log->success("Adding session {}\n", *new_session);
            }
            else
                Log->warning("Session limit reached, not adding session {}\n", *new_session);
        }
        catch (std::exception& e)
        {
            Log->error("Error accepting connection: {}\n", e.what());
        }
        co_return;
    };

    for (;;)
    {
        auto socket = co_await acceptor_.async_accept(asio::use_awaitable);
        asio::co_spawn(executor, accept_impl(std::move(socket)), asio::detached);
    }
}

void server::alive_checker() noexcept
{
    // Run alive checker every 60 seconds
    constexpr auto check_interval = std::chrono::seconds{ 60 };
    while (is_running_)
    {
        asio::steady_timer timer{ io_context_, check_interval };
        timer.wait();

        Log->message("server::alive_checker is running...\n");
        std::vector<session::id_type> sessions_to_erase;
        std::mutex mutex;

        for_each_session([&mutex, &sessions_to_erase](const std::shared_ptr<session>& session)
            -> asio::awaitable<void>
        {
            const auto token = utils::generate_random_string(16);
            co_await session->execute_command("echo " + token);
            if (!session->is_alive())
            {
                std::lock_guard lock{ mutex };
                sessions_to_erase.push_back(session->id());
            }
            co_return;
        });

        for (const auto& id : sessions_to_erase)
            remove_session(id);
        Log->message("server::alive_checker done!\n");
    }
}
