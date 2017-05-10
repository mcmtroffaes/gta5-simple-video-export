#pragma once

#include "../spdlog/include/spdlog/spdlog.h"

#define SCRIPT_NAME "SimpleVideoExport"

class Settings
{
public:
	static const std::wstring ini_filename_;
	bool enable_;
	std::wstring exportfolder_;
	spdlog::level::level_enum log_level_;
	spdlog::level::level_enum log_flush_on_;
	unsigned long log_max_file_size_;
	unsigned long log_max_files_;
	std::wstring raw_folder_;
	std::wstring raw_video_filename_;
	std::wstring raw_audio_filename_;
	std::wstring client_batchfile_;
	std::wstring client_executable_;
	std::wstring client_args_;
	std::map<std::wstring, std::wstring> audioformats_;
	std::map<std::wstring, std::wstring> videoformats_;
	Settings();
};

/* declaration resides in dllmain.cpp */
extern std::unique_ptr<Settings> settings;
