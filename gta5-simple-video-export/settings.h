#pragma once

#include <memory>

#include "../spdlog/include/spdlog/spdlog.h"
#include "../ini-parser/ini.hpp"

#define SCRIPT_NAME "simple-video-export"
#define SCRIPT_FOLDER "SVE"

class Settings
{
public:
	static const std::string ini_filename_;
	std::string output_folder_;
	spdlog::level::level_enum log_level_;
	spdlog::level::level_enum log_flush_on_;

	Settings();
	bool Load();
};

/* declaration resides in dllmain.cpp */
extern std::unique_ptr<Settings> settings;
