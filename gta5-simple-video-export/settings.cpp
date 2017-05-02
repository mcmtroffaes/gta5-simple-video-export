#include "settings.h"
#include "logger.h"

const std::string Settings::ini_filename_ = SCRIPT_FOLDER "\\config.ini";

Settings::Settings()
	: output_folder_(), log_level_(spdlog::level::info), log_flush_on_(spdlog::level::off)
{
}

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
	LOG_ENTER;
	auto success = false;
	auto keyvalue = level.values.find(name);
	if (keyvalue == level.values.end()) {
		logger->debug("{} not set", name);
	}
	else {
		auto value_str = keyvalue->second;
		success = convert(value_str, value);
		if (success) {
			logger->debug("{} = {}", name, value_str);
		}
		else {
			logger->error("{} = parse error ({})", name, value_str);
		}
	}
	LOG_EXIT;
	return success;
}

bool Settings::Load() {
	LOG_ENTER;
	INI::Parser parser(ini_filename_.c_str());
	log_level_ = spdlog::level::info;
	parse(parser.top(), "log_level", log_level_);
	parse(parser.top(), "log_flush_on", log_flush_on_);
	if (!parse(parser.top(), "output_folder", output_folder_)) {
		logger->error("mod disabled as output_folder is missing");
		LOG_EXIT;
		return false;
	}
	LOG_EXIT;
	return true;
}
