#pragma once

#include "logger.h"
#include <inipp.h>

#define SCRIPT_NAME "SimpleVideoExport"

class Settings : public inipp::Ini<wchar_t>
{
public:
	static const std::wstring ini_filename_;
	Settings();

	const Section & GetSec(const std::wstring & sec_name) const;

	template <typename T>
	bool GetVar(const Section & sec, const std::wstring & var_name, T & value) const
	{
		LOG_ENTER;
		bool found = false;
		auto var = sec.find(var_name);
		if (var == sec.end()) {
			LOG->warn(L"variable {} not found", var_name);
		}
		else {
			found = inipp::extract(var->second, value);
			if (!found) {
				LOG->error(L"failed to parse {}", var->second);
			}
		}
		return found;
		LOG_EXIT;
	}
};

/* declaration resides in dllmain.cpp */
extern std::unique_ptr<Settings> settings;
