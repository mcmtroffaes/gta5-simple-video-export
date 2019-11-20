#pragma once

#include "avcreate.h"
#include "logger.h"
#include <filesystem>
#include <inipp.h>

extern "C" {
#include <libavcodec/avcodec.h>
}

#define SCRIPT_NAME "SimpleVideoExport"

class Settings : public inipp::Ini<char>
{
public:
	static const std::filesystem::path ini_filename_;
	std::filesystem::path export_filename;
	AVCodecID video_codec_id;
	AVDictionaryPtr video_codec_options;
	AVCodecID audio_codec_id;
	AVDictionaryPtr audio_codec_options;

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
		LOG_EXIT;
		return found;
	}
};

/* declaration resides in dllmain.cpp */
extern std::unique_ptr<Settings> settings;
