#pragma once

#include "../spdlog/include/spdlog/spdlog.h"

/* declaration resides in dllmain.cpp */
extern std::shared_ptr<spdlog::logger> logger;

#define LOG_INFO(msg) logger->info("{}: {}", __func__, msg)
#define LOG_DEBUG(msg) logger->debug("{}: {}", __func__, msg)
#define LOG_ERROR(msg) logger->error("{}: {}", __func__, msg)
#define LOG_TRACE(msg) logger->trace("{}: {}", __func__, msg)
#define LOG_ENTER LOG_TRACE("enter")
#define LOG_EXIT LOG_TRACE("exit")
