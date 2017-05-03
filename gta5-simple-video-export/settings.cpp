#include "settings.h"
#include "logger.h"
#include "../ini-parser/ini.hpp"

// note: in this entire code, logger can be nullptr
// because we need to read the settings before the logger is set up
// when the mod is initialised

bool convert(const std::string & value_str, std::string & value)
{
	value = value_str;
	return true;
}

bool convert(const std::string & value_str, bool & value) {
	if (value_str == "true") {
		value = true;
		return true;
	}
	else if (value_str == "false") {
		value = false;
		return true;
	}
	else {
		return false;
	}
}

bool convert(const std::string & value_str, spdlog::level::level_enum & value) {
	if (value_str == "trace") {
		value = spdlog::level::trace;
		return true;
	}
	if (value_str == "debug") {
		value = spdlog::level::debug;
		return true;
	}
	if (value_str == "info") {
		value = spdlog::level::info;
		return true;
	}
	if (value_str == "warn") {
		value = spdlog::level::warn;
		return true;
	}
	if (value_str == "err") {
		value = spdlog::level::err;
		return true;
	}
	if (value_str == "critical") {
		value = spdlog::level::critical;
		return true;
	}
	if (value_str == "off") {
		value = spdlog::level::off;
		return true;
	}
	return false;
}

template <typename T>
bool parse(const INI::Level & level, const std::string & name, T & value)
{
	// note: logger may not have been initialized yet, need to check each time
	SAFE_LOG_ENTER;
	auto success = false;
	auto keyvalue = level.values.find(name);
	if (keyvalue == level.values.end()) {
		if (logger) logger->debug("{} not set", name);
	}
	else {
		auto value_str = keyvalue->second;
		success = convert(value_str, value);
		if (success) {
			if (logger) logger->debug("{} = {}", name, value_str);
		}
		else {
			if (logger) logger->error("{} = parse error ({})", name, value_str);
		}
	}
	SAFE_LOG_EXIT;
	return success;
}

BOOL DirectoryExists(LPCSTR szPath)
{
	DWORD dwAttrib = GetFileAttributesA(szPath);
	return (dwAttrib != INVALID_FILE_ATTRIBUTES && (dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

const std::string Settings::ini_filename_ = SCRIPT_FOLDER "\\config.ini";

Settings::Settings()
	: log_level_(spdlog::level::info)
	, log_flush_on_(spdlog::level::off)
	, output_folder_()
{
	SAFE_LOG_ENTER;
	std::unique_ptr<INI::Parser> parser = nullptr;
	try {
		parser.reset(new INI::Parser(ini_filename_.c_str()));
	}
	catch (const std::exception & ex) {
		if (logger) logger->error("{} parse error ({})", ini_filename_, ex.what());
	}
	catch (...) {
		if (logger) logger->error("{} unknown parse error", ini_filename_);
	}
	if (parser) {
		parse(parser->top(), "log_level", log_level_);
		parse(parser->top(), "log_flush_on", log_flush_on_);
		if (parse(parser->top(), "output_folder", output_folder_)) {
			if (!DirectoryExists(output_folder_.c_str())) {
				if (logger) logger->error("output_folder {} does not exist", output_folder_);
				output_folder_ = "";
			}
		}
		else {
			if (logger) logger->error("no output_folder specified in {}", ini_filename_);
		}
	}
	SAFE_LOG_EXIT;
}
