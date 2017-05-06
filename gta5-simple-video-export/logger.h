#pragma once

#include "../spdlog/include/spdlog/spdlog.h"

/* declaration resides in dllmain.cpp */
extern std::shared_ptr<spdlog::logger> logger;

#define LOG if (logger) logger
#define LOG_ENTER LOG->trace("{}: enter", __func__)
#define LOG_EXIT LOG->trace("{}: exit", __func__)
