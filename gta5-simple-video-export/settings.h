#pragma once

#include "../spdlog/include/spdlog/spdlog.h"

#define SCRIPT_NAME "SimpleVideoExport"

class Settings
{
public:
	static const std::string ini_filename_;
	bool enable_;
	std::string exportfolder_;
	spdlog::level::level_enum log_level_;
	spdlog::level::level_enum log_flush_on_;
	unsigned long log_max_file_size_;
	unsigned long log_max_files_;
	std::string raw_folder_;
	std::string raw_video_filename_;
	std::string raw_audio_filename_;
	std::string client_batchfile_;
	std::string client_executable_;
	std::string client_args_;
	std::map<std::string, std::string> audioformats_;
	std::map<std::string, std::string> videoformats_;
	Settings();
};

/* declaration resides in dllmain.cpp */
extern std::unique_ptr<Settings> settings;
