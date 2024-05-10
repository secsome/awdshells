#include "logger.hpp"
#include "server.hpp"
#include "cli.hpp"
#include "version.hpp"

#include <argparse.hpp>

#include <cstdio>
#include <memory>
#include <print>
#include <thread>

std::unique_ptr<logger> Log;
std::unique_ptr<server> Manager;

int main(int argc, const char* argv[])
{
    argparse::ArgumentParser parser{ PROJECT_NAME, VERSION_STR };
    parser.add_argument("-h", "--help").help("Show this help message and exit.").default_value(false).implicit_value(true);
    parser.add_argument("-v", "--version").help("Show version").default_value(false).implicit_value(true);
    parser.add_argument("-l", "--level").help("Set log level(raw, success, message, warning, error)").default_value("success");
    // parser.add_argument("-a", "--address").help("Set server address").default_value("0.0.0.0");
    parser.add_argument("-p", "--port").help("Set server port").default_value(11451);
    parser.add_argument("-c", "--concurrency").help("Set server concurrency").default_value(50);
    parser.add_argument("-g", "--gui").help("Enable GUI").default_value(false).implicit_value(true);
    parser.parse_args(argc, argv);
    
    if (parser.get<bool>("--help"))
    {
        std::println("{}", parser.help().str());
        return 0;
    }

    if (parser.get<bool>("--version"))
    {
        std::println(PROJECT_FULL_NAME);
        return 0;
    }

    logger::level log_level;
    std::string level_str = parser.get<std::string>("--level");
    if (level_str == "success")
        log_level = logger::level::success;
    else if (level_str == "message")
        log_level = logger::level::message;
    else if (level_str == "warning")
        log_level = logger::level::warning;
    else if (level_str == "error")
        log_level = logger::level::error;
    else if (level_str == "raw")
        log_level = logger::level::raw;
    else if (level_str == "none")
        log_level = logger::level::none;
    else
    {
        std::println("Invalid log level.");
        return 1;
    }

    const auto current_time = std::chrono::system_clock::now();
    std::string log_file_name = std::format("awdshells-{:%Y%m%d%H%M%S}.log", current_time);
    Log = std::make_unique<logger>(log_file_name, log_level);
    if (Log == nullptr)
    {
        std::println("Failed to create logger instance.");
        return 1;
    }

    const auto address = "0.0.0.0";
    const auto port = parser.get<int>("--port");
    const auto concurrency = parser.get<int>("--concurrency");
    Log->message("Starting server... on {}:{}, with {} as concurrency hint\n", address, port, concurrency);
    Manager = std::make_unique<server>(address, port, concurrency);
    if (Manager == nullptr)
    {
        Log->error("Failed to create server instance.\n");
        return 1;
    }

    Log->success("Server started.\n");
    Log->message("Receiving sessions...\n");

    std::thread server_thread{ &server::start, Manager.get() };

    if (parser.get<bool>("--gui"))
    {
        Log->message("Starting GUI...\n");
        // TODO: GUI
        Log->message("GUI stopped.\n");
    }
    else
    {
        Log->message("Starting CLI...\n");
        cli cmd;
        cmd.run();
        Log->message("CLI stopped.\n");
    }

    Log->message("Stopping server...\n");
    Manager->stop();
    server_thread.join();
    Log->success("Server stopped.\n");

    return 0;
}
