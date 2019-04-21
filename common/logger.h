#pragma once

#define SPDLOG_WCHAR_TO_UTF8_SUPPORT

#include "spdlog/spdlog.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"

/* declaration resides in dllmain.cpp */
extern std::shared_ptr<spdlog::logger> logger;

// parse spdlog::level
std::wistream & operator >> (std::wistream & is, spdlog::level::level_enum & value);

// set av_log level
// call this whenever you change the logger level
void AVLogSetLevel(spdlog::level::level_enum level);

// set up callback to av_log will send log messages to the logger instead of stderr
// call this once at the start of your application
void AVLogSetCallback();

#define LOG if (logger) logger
#define LOG_ENTER LOG->trace("{}: enter", __func__)
#define LOG_EXIT LOG->trace("{}: exit", __func__)
