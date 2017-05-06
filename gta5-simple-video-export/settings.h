#pragma once

#include "../spdlog/include/spdlog/spdlog.h"

#define SCRIPT_NAME "simple-video-export"
#define SCRIPT_FOLDER "SVE"

class Settings
{
public:
	static const std::string ini_filename_;
	bool enable_;
	spdlog::level::level_enum log_level_;
	spdlog::level::level_enum log_flush_on_;
	std::string raw_folder_;
	std::string raw_video_filename_;
	std::string raw_audio_filename_;
	std::string client_executable_;
	std::string client_args_;
	Settings();
	bool IsRawFolderPipe() const { return raw_folder_ == "\\\\.\\pipe\\"; };
};

/* declaration resides in dllmain.cpp */
extern std::unique_ptr<Settings> settings;
