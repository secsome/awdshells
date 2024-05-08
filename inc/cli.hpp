#pragma once

#define CTRL_C_HANDLER 0

#include "batch.hpp"

#include <vector>
#include <string>
#include <unordered_map>
#include <map>
#include <functional>
#include <span>

class cli
{
public:
    using command_handler_type = std::function<void(std::span<std::string>)>;

    explicit cli() noexcept;
    ~cli() noexcept = default;

    cli(const cli&) = delete;
    cli& operator=(const cli&) = delete;

    cli(cli&&) noexcept = delete;
    cli& operator=(cli&&) noexcept = delete;

    void run() noexcept;

private:
    // handlers
    void handle_session(std::span<std::string> args) noexcept;
    void handle_batch(std::span<std::string> args) noexcept;
    void handle_clear(std::span<std::string> args) noexcept;
    void handle_log(std::span<std::string> args) noexcept;

private:
#if CTRL_C_HANDLER
    bool setup_ctrlc_handler() noexcept;
#endif

    bool handle_args(std::span<std::string> args) noexcept;
    void register_command(const std::string& command, command_handler_type handler) noexcept;

    std::string& translate_command_shortcuts(std::string& command) noexcept;

    std::unordered_map<std::string, command_handler_type> handlers_;
    batch::id_type current_batch_id_;
    std::map<batch::id_type, std::shared_ptr<batch>> batches_;
};