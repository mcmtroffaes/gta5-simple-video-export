#pragma once

#include "../spdlog/include/spdlog/spdlog.h"

#define SCRIPT_NAME "simple-video-export"
#define SCRIPT_FOLDER "SVE"

class Settings
{
public:
	static const std::string ini_filename_;
	spdlog::level::level_enum log_level_;
	spdlog::level::level_enum log_flush_on_;
	std::string output_folder_;
	Settings();
};

/* declaration resides in dllmain.cpp */
extern std::unique_ptr<Settings> settings;
