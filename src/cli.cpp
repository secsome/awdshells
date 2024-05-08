#include "cli.hpp"

#include "externs.hpp"
#include "utils.hpp"

#include <cstdio>
#include <iostream>
#include <print>
#include <span>
#include <charconv>

void cli::register_command(const std::string& command, command_handler_type handler) noexcept
{
    auto it = handlers_.find(command);
    if (it != handlers_.end())
    {
        Log->warning("Command {} already registered. Overwriting...\n", command);
        it->second = handler;
    }
    else
        handlers_.emplace(command, handler);
}

static bool is_running;

// Register commands here
cli::cli() noexcept
    : handlers_{}
    , current_batch_id_{ 0 }
    , batches_{}
{
    register_command("session", std::bind(&cli::handle_session, this, std::placeholders::_1));
    register_command("batch", std::bind(&cli::handle_batch, this, std::placeholders::_1));
    register_command("clear", std::bind(&cli::handle_clear, this, std::placeholders::_1));
    register_command("log", std::bind(&cli::handle_log, this, std::placeholders::_1));
}

void cli::run() noexcept
{
#if CTRL_C_HANDLER
    if (!setup_ctrlc_handler())
    {
        Log->error("Failed to setup Ctrl-C handler.\n");
        return;
    }
#endif

    is_running = true;

    while (is_running)
    {
        std::print("awdshells> ");
        std::string command;
        do
        {
            std::string line;
            std::getline(std::cin, line);
            if (line.ends_with('\\'))
            {
                command += line.substr(0, line.length() - 1);
                std::print("> ");
            }
            else
            {
                command += line;
                break;
            }
        } while (true);
        utils::trim(command, "\r\n ");
        if (command.empty())
            continue;

        // convert the range to a vector
        auto&& args = utils::split(command, " ");
        is_running = handle_args(args);
    }
}

static void ctrlc_implement() noexcept
{
    Log->message("Ctrl-C pressed. Exiting program...\n");
    is_running = false;
}

#if CTRL_C_HANDLER
#ifdef _WIN32
#include <Windows.h>
static BOOL WINAPI _ctrl_c_handler(DWORD fdwCtrlType)
{
    if (fdwCtrlType == CTRL_C_EVENT)
        ctrlc_implement();
    return TRUE;
}

bool cli::setup_ctrlc_handler() noexcept
{
    return SetConsoleCtrlHandler(_ctrl_c_handler, TRUE);
}
#else
#include <signal.h>
#include <unistd.h>

static void _ctrl_c_handler(int s)
{
    ctrlc_implement();
}

bool cli::setup_ctrlc_handler() noexcept
{
    struct sigaction handler;
    handler.sa_handler = _ctrl_c_handler;
    sigemptyset(&handler.sa_mask);
    handler.sa_flags = 0;
    sigaction(SIGINT, &handler, nullptr);
}
#endif
#endif

bool cli::handle_args(std::span<std::string> args) noexcept
{
    if (args.empty())
        return true;

    std::string command = args.front();
    args = args.subspan(1);

    translate_command_shortcuts(command);

    if (command == "exit")
    {
        // If there are active sessions, ask for confirmation
        if (Manager->session_count())
        {
            std::println("Are you sure you want to exit? (y/n)");
            std::string response;
            std::cin >> response;
            if (response == "y")
            {
                Log->message("Exiting program...\n");
                return false;
            }
            else
                return true;
        }
        else
        {
            Log->message("Exiting program...\n");
            return false;
        }
    }

    if (auto it = handlers_.find(command); it != handlers_.end())
        it->second(args);
    else
        Log->warning("{}: no such command\n", command);
    
    return true;
}

std::string& cli::translate_command_shortcuts(std::string& command) noexcept
{
    if (command == "sess" || command == "s")
        command = "session";
    else if (command == "bat" || command == "b")
        command = "batch";
    else if (command == "clr" || command == "c")
        command = "clear";
    else if (command == "l")
        command = "log";

    return command;
}

void cli::handle_session(std::span<std::string> args) noexcept
{
    if (args.empty())
    {
        Log->warning("session: no subcommand specified\n");
        return;
    }

    if (args[0] == "-l")
    {
        if (args.size() > 1 && args[1] == "all")
        {
            Log->message("Listing all sessions...\n");
            std::println("{}", *Manager);
        }
        else
        {
            Log->message("Listing active sessions...\n");
            std::println("{:a}", *Manager);
        }
    }
    else if (args[0] == "-i")
    {
        Log->message("Interacting with session...\n");
        if (args.size() < 2)
        {
            Log->warning("session: no session id specified\n");
            return;
        }

        size_t session_id;
        auto [ptr, ec] = std::from_chars(args[1].data(), args[1].data() + args[1].size(), session_id);
        if (ec == std::errc{})
        {
            const auto session = Manager->get_session(session_id);
            if (session == nullptr)
                Log->warning("session: session {} not found\n", session_id);
            else
                session->interact();
        }
        else
            Log->warning("session: invalid session id '{}'\n", args[1]);

    }
    else if (args[0] == "-a")
    {
        if (args.size() < 2)
        {
            Log->warning("session: no command specified\n");
            return;
        }

        Log->message("Execute command on all sessions...\n");
        std::string command_to_execute;
        for (size_t i = 1; i < args.size(); ++i)
            command_to_execute += args[i] + " ";

        Manager->for_each_session([args, command_to_execute](const std::shared_ptr<session>& session)
            -> asio::awaitable<void>
        {
            const auto result = co_await session->execute_command(command_to_execute);
            std::print("{}", result);
        });
    }
    else
        Log->warning("session: unknown subcommand '{}'\n", args[0]);
}

void cli::handle_clear(std::span<std::string> args) noexcept
{
    std::vector<session::id_type> sessions_to_erase;
    std::mutex mutex;
    if (args.empty())
    {
        Manager->for_each_session([&mutex, &sessions_to_erase](const std::shared_ptr<session>& session)
            -> asio::awaitable<void>
        {
            if (!session->is_alive())
            {
                std::lock_guard lock{ mutex };
                sessions_to_erase.push_back(session->id());
            }
            co_return;
        }, false);
    }
    else if (args[0] == "-a")
    {
        Manager->for_each_session([&mutex, &sessions_to_erase](const std::shared_ptr<session>& session)
            -> asio::awaitable<void>
        {
            const auto token = utils::generate_random_string(16);
            auto&& result = co_await session->execute_command("echo " + token);
            if (!session->is_alive() || session->is_timeout())
            {
                std::lock_guard lock{ mutex };
                sessions_to_erase.push_back(session->id());
            }
            co_return;
        }, false);
    }

    for (const auto& id : sessions_to_erase)
        Manager->remove_session(id);
}

void cli::handle_batch(std::span<std::string> args) noexcept
{
    if (args.empty())
    {
        Log->warning("batch: no subcommand specified\n");
        return;
    }

    if (args[0] == "create")
    {
        Log->message("Creating new batch...\n");
        auto new_batch = std::make_shared<batch>(current_batch_id_);
        batches_.emplace(current_batch_id_, new_batch);
        ++current_batch_id_;
    }
    else if (args[0] == "delete")
    {
        if (args.size() < 2)
        {
            Log->warning("batch: no batch id specified\n");
            return;
        }

        size_t batch_id;
        auto [ptr, ec] = std::from_chars(args[1].data(), args[1].data() + args[1].size(), batch_id);
        if (ec == std::errc{})
        {
            const auto itr = batches_.find(batch_id);
            if (itr == batches_.end())
                Log->warning("batch: batch {} not found\n", batch_id);
            else
            {
                Log->message("Removing batch {}\n", *itr->second);
                batches_.erase(itr);
            }
        }
        else
            Log->warning("batch: invalid batch id '{}'\n", args[1]);
    }
    else if (args[0] == "list")
    {
        Log->message("Listing all batches...\n");
        for (const auto& [id, batch] : batches_)
            std::println("{}", *batch);
    }
    else if (args[0] == "operate")
    {
        if (args.size() < 2)
        {
            Log->warning("batch: no batch id specified\n");
            return;
        }

        size_t batch_id;
        auto [ptr, ec] = std::from_chars(args[1].data(), args[1].data() + args[1].size(), batch_id);
        if (ec == std::errc{})
        {
            const auto itr = batches_.find(batch_id);
            if (itr == batches_.end())
                Log->warning("batch: batch {} not found\n", batch_id);
            else
                itr->second->operate();
        }
        else
            Log->warning("batch: invalid batch id '{}'\n", args[1]);
    }
    else if (args[0] == "clear")
    {
        Log->message("Clearing all batches...\n");
        batches_.clear();
    }
    else
        Log->warning("batch: unknown subcommand '{}'\n", args[0]);
}

void cli::handle_log(std::span<std::string> args) noexcept
{
    if (args.empty())
    {
        Log->warning("log: no loglevel specified\n");
        return;
    }

    if (args[0] == "raw")
    {
        Log->message("Setting display log level to raw\n");
        Log->set_display_level(logger::level::raw);
    }
    else if (args[0] == "success")
    {
        Log->message("Setting display log level to success\n");
        Log->set_display_level(logger::level::success);
    }
    else if (args[0] == "message")
    {
        Log->message("Setting display log level to message\n");
        Log->set_display_level(logger::level::message);
    }
    else if (args[0] == "warning")
    {
        Log->message("Setting display log level to warning\n");
        Log->set_display_level(logger::level::warning);
    }
    else if (args[0] == "error")
    {
        Log->message("Setting display log level to error\n");
        Log->set_display_level(logger::level::error);
    }
    else if (args[0] == "none")
    {
        Log->message("Setting display log level to none\n");
        Log->set_display_level(logger::level::none);
    }
    else
        Log->warning("log: unknown loglevel '{}'\n", args[0]);
}