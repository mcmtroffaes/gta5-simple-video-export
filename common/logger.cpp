#include "logger.h"

#include <string>

std::wistream & operator >> (std::wistream & is, spdlog::level::level_enum & value)
{
	std::wstring value_str;
	is >> value_str;
	if (value_str == L"trace") {
		value = spdlog::level::trace;
	}
	else if (value_str == L"debug") {
		value = spdlog::level::debug;
	}
	else if (value_str == L"info") {
		value = spdlog::level::info;
	}
	else if (value_str == L"warn") {
		value = spdlog::level::warn;
	}
	else if (value_str == L"err") {
		value = spdlog::level::err;
	}
	else if (value_str == L"critical") {
		value = spdlog::level::critical;
	}
	else if (value_str == L"off") {
		value = spdlog::level::off;
	}
	else {
		is.setstate(std::ios::failbit);
	}
	return is;
}
