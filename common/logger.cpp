#include "logger.h"

#include <iostream>
#include <string>

extern "C" {
#include <libavutil/log.h>
}

#include <polyhook2/ErrorLog.hpp>

std::istream & operator >> (std::istream & is, spdlog::level::level_enum & value)
{
	std::string value_str;
	is >> value_str;
	if (value_str == "trace") {
		value = spdlog::level::trace;
	}
	else if (value_str == "debug") {
		value = spdlog::level::debug;
	}
	else if (value_str == "info") {
		value = spdlog::level::info;
	}
	else if (value_str == "warn") {
		value = spdlog::level::warn;
	}
	else if (value_str == "err") {
		value = spdlog::level::err;
	}
	else if (value_str == "critical") {
		value = spdlog::level::critical;
	}
	else if (value_str == "off") {
		value = spdlog::level::off;
	}
	else {
		is.setstate(std::ios::failbit);
	}
	return is;
}

auto spdlog_av_level(spdlog::level::level_enum level) {
	switch (level) {
	case spdlog::level::trace:
		return AV_LOG_TRACE;
	case spdlog::level::debug:
		return AV_LOG_DEBUG;
	case spdlog::level::info:
		return AV_LOG_INFO;
	case spdlog::level::warn:
		return AV_LOG_WARNING;
	case spdlog::level::err:
		return AV_LOG_ERROR;
	case spdlog::level::critical:
		return AV_LOG_FATAL;
	case spdlog::level::off:
		return AV_LOG_QUIET;
	default:
		return AV_LOG_INFO;
	}
}

auto av_spdlog_level(int level) {
	switch (level) {
	case AV_LOG_TRACE:
		return spdlog::level::trace;
	case AV_LOG_DEBUG:
	case AV_LOG_VERBOSE:
		return spdlog::level::debug;
	case AV_LOG_INFO:
		return spdlog::level::info;
	case AV_LOG_WARNING:
		return spdlog::level::warn;
	case AV_LOG_ERROR:
		return spdlog::level::err;
	case AV_LOG_FATAL:
	case AV_LOG_PANIC:
		return spdlog::level::critical;
	case AV_LOG_QUIET:
		return spdlog::level::off;
	default:
		return spdlog::level::info;
	}
}

void AVLogCallback(void* avcl, int level, const char* fmt, va_list vl)
{
	// each thread should have its own character buffer
	thread_local char line[256] = { 0 };
	thread_local int pos = 0;

	int print_prefix = 1;
	int remain = sizeof(line) - pos;
	if (remain > 0) {
		int ret = av_log_format_line2(avcl, level, fmt, vl, line + pos, remain, &print_prefix);
		if (ret >= 0) {
			pos += (ret <= remain) ? ret : remain;
		}
		else {
			// log at the specified level rather than error level to avoid spamming the log
			LOG->log(av_spdlog_level(level), "failed to format av_log message: {}", fmt);
		}
	}
	// only write log message on newline
	size_t i = strnlen(fmt, sizeof(line));
	if ((i > 0) && (fmt[i - 1] == '\n')) {
		// remove newline (spdlog adds a newline automatically)
		if ((pos > 0) && (line[pos - 1] == '\n')) {
			line[pos - 1] = '\0';
		}
		LOG->log(av_spdlog_level(level), line);
		pos = 0;
		*line = '\0';
	}
}

void AVLogSetLevel(spdlog::level::level_enum level)
{
	av_log_set_level(spdlog_av_level(level));
}

void AVLogSetCallback()
{
	av_log_set_callback(AVLogCallback);
}

std::string AVErrorString(int errnum) {
	char buffer[AV_ERROR_MAX_STRING_SIZE] = { 0 };
	av_make_error_string(buffer, sizeof(buffer), errnum);
	return std::string(buffer);
}

auto plh_spdlog_level(PLH::ErrorLevel level) {
	switch (level) {
	case PLH::ErrorLevel::INFO:
		return spdlog::level::info;
	case PLH::ErrorLevel::WARN:
		return spdlog::level::warn;
	case PLH::ErrorLevel::SEV:
		return spdlog::level::err;
	case PLH::ErrorLevel::NONE:
		return spdlog::level::off;
	default:
		return spdlog::level::info;
	}
}

auto spdlog_plh_level(spdlog::level::level_enum level) {
	switch (level) {
	case spdlog::level::trace:
	case spdlog::level::debug:
	case spdlog::level::info:
		return PLH::ErrorLevel::INFO;
	case spdlog::level::warn:
		return PLH::ErrorLevel::WARN;
	case spdlog::level::err:
	case spdlog::level::critical:
		return PLH::ErrorLevel::SEV;
	case spdlog::level::off:
		return PLH::ErrorLevel::NONE;
	default:
		return PLH::ErrorLevel::INFO;
	}
}

void PLHLogSetLevel(spdlog::level::level_enum level)
{
	PLH::ErrorLog::singleton().setLogLevel(spdlog_plh_level(level));
}

void PLHLog()
{
	auto err = PLH::ErrorLog::singleton().pop();
	while (!err.msg.empty()) {
		LOG->log(plh_spdlog_level(err.lvl), err.msg);
		err = PLH::ErrorLog::singleton().pop();
	}
}
