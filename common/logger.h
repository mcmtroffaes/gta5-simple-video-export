#pragma once

#define SPDLOG_WCHAR_TO_UTF8_SUPPORT

#include "spdlog/spdlog.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"

/* declaration resides in dllmain.cpp */
extern std::shared_ptr<spdlog::logger> logger;

// convert wstring to UTF-8 string
std::string wstring_to_utf8(const std::wstring & str);

// convert UTF-8 string to wstring
std::wstring wstring_from_utf8(const std::string& str);

// parse spdlog::level
std::wistream & operator >> (std::wistream & is, spdlog::level::level_enum & value);

#define LOG if (logger) logger
#define LOG_ENTER LOG->trace("{}: enter", __func__)
#define LOG_EXIT LOG->trace("{}: exit", __func__)
