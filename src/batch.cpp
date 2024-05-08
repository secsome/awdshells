#include "batch.hpp"

#include "externs.hpp"
#include "utils.hpp"

#include <print>
#include <charconv>

void batch::execute(std::string command) noexcept
{
    Log->message("Executing command for current batch...\n");
    for_each_session([command](const std::shared_ptr<session>& session) -> asio::awaitable<void>
    {
        const auto&& response = co_await session->execute_command(command);
        std::print("{}", response);
        co_return;
    });
}

void batch::upload(std::filesystem::path filepath, std::string target_name) noexcept
{
    std::ifstream file{ filepath, std::ios::binary };
    if (!file.is_open())
    {
        Log->error("Failed to open file: {}\n", filepath.string());
        return;
    }

    // Read whole file into memory
    file.seekg(0, std::ios::end);
    const auto file_size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<char> buffer(file_size);
    file.read(buffer.data(), file_size);
    file.close();

    std::string echo_str;
    for (const auto& c : buffer)
    {
        echo_str += "\\x";
        echo_str += std::format("{:02x}", static_cast<unsigned char>(c));
    }

    Log->message("Uploading file for current batch...\n");
    for_each_session([target_name, echo_str](const std::shared_ptr<session>& session) -> asio::awaitable<void>
    {
        constexpr size_t ECHO_BLOCK_SIZE = 512;

        const std::string clear_command = "echo -ne \"\" > " + target_name;
        co_await session->execute_command(clear_command);

        for (size_t bytes_written = 0; bytes_written < echo_str.size(); bytes_written += ECHO_BLOCK_SIZE)
        {
            const auto block = std::string_view{ echo_str.data() + bytes_written, std::min(echo_str.length() - bytes_written, ECHO_BLOCK_SIZE) };
            std::string block_command = "echo -ne \"";
            block_command += block;
            block_command += "\" >> ";
            block_command += target_name;
            co_await session->execute_command(block_command);
        }
        co_return;
    });
}

void batch::list() noexcept
{
    Log->message("Listing sessions for current batch...\n");

    for_each_session([](const std::shared_ptr<session>& session) -> asio::awaitable<void>
    {
        std::print("{}\n", *session);
        co_return;
    });
}

void batch::operate() noexcept
{
    Log->message("Operating on batch {}\n", *this);

    std::string command;
    while (true)
    {
        std::print("batch> ");
        std::getline(std::cin, command);

        const auto args = utils::split(command, " ");

        if (args.empty())
            continue;

        if (args[0] == "exit")
            break;
        else if (args[0] == "add")
        {
            size_t session_id;
            auto [ptr, ec] = std::from_chars(args[1].data(), args[1].data() + args[1].size(), session_id);
            if (ec == std::errc{})
            {
                const auto session = Manager->get_session(session_id);
                if (session == nullptr)
                    Log->warning("batch: session {} not found\n", session_id);
                else
                    add_session(session_id);
            }
            else
                Log->warning("Usage: add <session_id>\n");
        }
        else if (args[0] == "remove")
        {
            size_t session_id;
            auto [ptr, ec] = std::from_chars(args[1].data(), args[1].data() + args[1].size(), session_id);
            if (ec == std::errc{})
            {
                const auto session = Manager->get_session(session_id);
                if (session == nullptr)
                    Log->warning("batch: session {} not found\n", session_id);
                else
                    remove_session(session_id);
            }
            else
                Log->warning("Usage: remove <session_id>\n");
        }
        else if (args[0] == "list")
            list();
        else if (args[0] == "upload")
        {
            if (args.size() < 3)
            {
                Log->warning("Usage: upload <filepath> <target_name>\n");
                continue;
            }
            upload(args[1], args[2]);
        }
        else if (args[0] == "execute")
        {
            if (args.size() < 2)
            {
                Log->warning("Usage: execute <command>\n");
                continue;
            }
            std::string full_command;
            for (size_t i = 1; i < args.size(); ++i)
            {
                full_command += args[i];
                full_command += ' ';
            }
            execute(full_command);
        }
        else
        {
            Log->warning(
                "Unknown command: {}\n"
                "Available commands: add, remove, list, upload, execute, exit\n"
                , args[0]);
        }
    }
}

