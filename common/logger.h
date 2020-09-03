#pragma once

#include <stdexcept>
#include <system_error>

#ifdef _WIN32
#define SPDLOG_WCHAR_TO_UTF8_SUPPORT
#endif

#include "spdlog/spdlog.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"

/* declaration resides in dllmain.cpp */
extern std::shared_ptr<spdlog::logger> logger;

// parse spdlog::level
std::istream & operator >> (std::istream & is, spdlog::level::level_enum & value);

// set av_log level
// call this whenever you change the logger level
void AVLogSetLevel(spdlog::level::level_enum level);

// set up callback to av_log will send log messages to the logger instead of stderr
// call this once at the start of your application
void AVLogSetCallback();

// convert ffmpeg errnum to std::string
std::string AVErrorString(int errnum);

// set polyhook logger callback
// call this once at the start of your application
void PLHLogSetCallback();

#define LOG if (logger) logger
#define LOG_ENTER LOG->trace("{}: enter", __func__)
#define LOG_EXIT LOG->trace("{}: exit", __func__)
#define LOG_ENTER_METHOD LOG->trace("{}::{}: enter", typeid(*this).name(), __func__)
#define LOG_EXIT_METHOD LOG->trace("{}::{}: exit", typeid(*this).name(), __func__)
#define THROW_FAILED(hrcall) { HRESULT _hr = S_OK; if (FAILED(_hr = (hrcall))) throw std::runtime_error(std::system_category().message(_hr)); };
#define LOG_CATCH catch (std::exception & e) { LOG->critical(e.what()); }
