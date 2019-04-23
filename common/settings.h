#pragma once

#include "logger.h"
#include <inipp.h>

#define SCRIPT_NAME "SimpleVideoExport"

class Settings : public inipp::Ini<char>
{
public:
	static const std::string ini_filename_;
	Settings();

	const Section & GetSec(const std::string & sec_name) const;

	template <typename T>
	bool GetVar(const Section & sec, const std::string & var_name, T & value) const
	{
		LOG_ENTER;
		bool found = false;
		auto var = sec.find(var_name);
		if (var == sec.end()) {
			LOG->warn("variable {} not found", var_name);
		}
		else {
			found = inipp::extract(var->second, value);
			if (!found) {
				LOG->error("failed to parse {}", var->second);
			}
		}
		return found;
		LOG_EXIT;
	}
};

/* declaration resides in dllmain.cpp */
extern std::unique_ptr<Settings> settings;
