#pragma once

#include "../spdlog/include/spdlog/spdlog.h"

/* declaration resides in dllmain.cpp */
extern std::shared_ptr<spdlog::logger> logger;

// convert wstring to UTF-8 string
std::string wstring_to_utf8(const std::wstring & str);

// parse spdlog::level
std::wistream & operator >> (std::wistream & is, spdlog::level::level_enum & value);

#define LOG if (logger) logger
#define LOG_ENTER LOG->trace("{}: enter", __func__)
#define LOG_EXIT LOG->trace("{}: exit", __func__)
