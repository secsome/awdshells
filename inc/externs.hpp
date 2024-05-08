#pragma once

#include <memory>

#include "logger.hpp"
#include "server.hpp"

extern std::unique_ptr<logger> Log;
extern std::unique_ptr<server> Manager;