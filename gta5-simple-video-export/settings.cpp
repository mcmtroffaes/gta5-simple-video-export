#include "settings.h"
#include "logger.h"

#include <inipp.h>
#include <fstream>
#include <sstream>

typedef inipp::Ini<wchar_t> Ini;

bool Convert(const std::wstring & value_str, std::wstring & value)
{
	value = value_str;
	return true;
}

bool Convert(const std::wstring & value_str, unsigned long & value) {
	try {
		value = std::stoul(value_str);
	}
	catch (...) {
		return false;
	}
	return true;
}

bool Convert(const std::wstring & value_str, bool & value) {
	if (value_str == L"true") {
		value = true;
		return true;
	}
	else if (value_str == L"false") {
		value = false;
		return true;
	}
	else {
		return false;
	}
}

bool Convert(const std::wstring & value_str, spdlog::level::level_enum & value) {
	if (value_str == L"trace") {
		value = spdlog::level::trace;
		return true;
	}
	if (value_str == L"debug") {
		value = spdlog::level::debug;
		return true;
	}
	if (value_str == L"info") {
		value = spdlog::level::info;
		return true;
	}
	if (value_str == L"warn") {
		value = spdlog::level::warn;
		return true;
	}
	if (value_str == L"err") {
		value = spdlog::level::err;
		return true;
	}
	if (value_str == L"critical") {
		value = spdlog::level::critical;
		return true;
	}
	if (value_str == L"off") {
		value = spdlog::level::off;
		return true;
	}
	return false;
}

template <typename T>
bool Parse(const Ini::Section & level, const std::wstring & name, T & value)
{
	LOG_ENTER;
	auto success = false;
	auto keyvalue = level.find(name);
	if (keyvalue == level.end()) {
		LOG->debug("value {} not found", wstring_to_utf8(name));
	}
	else {
		auto value_str = keyvalue->second;
		success = Convert(value_str, value);
		if (success) {
			LOG->debug("{} = {}", wstring_to_utf8(name), wstring_to_utf8(value_str));
		}
		else {
			LOG->error("{} = parse error ({})", wstring_to_utf8(name), wstring_to_utf8(value_str));
		}
	}
	LOG_EXIT;
	return success;
}

const Ini::Section & GetSection(const Ini & ini, const std::wstring & name) {
	LOG_ENTER;
	LOG->debug("parsing section [{}]", wstring_to_utf8(name));
	auto keyvalue = ini.sections.find(name);
	if (keyvalue != ini.sections.end()) {
		LOG_EXIT;
		return keyvalue->second;
	}
	else {
		LOG->error("section [{}] not found", wstring_to_utf8(name));
		LOG_EXIT;
		return Ini::Section();
	}
}

const std::wstring Settings::ini_filename_ = SCRIPT_NAME L".ini";

Settings::Settings()
// note: default settings here must match the default config.ini that is shipped
	: enable_(true)
	, exportfolder_(L"${videosfolder}")
	, log_level_(spdlog::level::info)
	, log_flush_on_(spdlog::level::off)
	, log_max_file_size_(10000000)
	, log_max_files_(5)
	, raw_folder_(L"${exportfolder}")
	, raw_video_filename_(L"sve-${timestamp}-video.yuv")
	, raw_audio_filename_(L"sve-${timestamp}-audio.raw")
	, client_batchfile_(L"${exportfolder}\\sve-${timestamp}.bat")
	, client_executable_(L"${exportfolder}\\ffmpeg.exe")
	, client_args_()
	, audioformats_()
	, videoformats_()
{
	LOG_ENTER;
	LOG->debug("parsing {}", wstring_to_utf8(ini_filename_));
	std::wifstream is(ini_filename_);
	Ini ini;
	ini.parse(is);
	if (!ini.errors.empty()) {
		for (auto error : ini.errors) {
			LOG->error("failed to parse \"{}\"", wstring_to_utf8(error));
		}
	}
	// always parse the [log] section first (even if mod is disabled)
	auto section_log = GetSection(ini, L"log");
	Parse(section_log, L"level", log_level_);
	Parse(section_log, L"flush_on", log_flush_on_);
	Parse(section_log, L"max_file_size", log_max_file_size_);
	Parse(section_log, L"max_files", log_max_files_);
	LOG->set_level(settings->log_level_);
	LOG->flush_on(settings->log_flush_on_);
	// now parse everything else
	auto section_default = GetSection(ini, L"DEFAULT");
	Parse(section_default, L"enable", enable_);
	Parse(section_default, L"exportfolder", exportfolder_);
	auto section_raw = GetSection(ini, L"raw");
	Parse(section_raw, L"folder", raw_folder_);
	Parse(section_raw, L"video_filename", raw_video_filename_);
	Parse(section_raw, L"audio_filename", raw_audio_filename_);
	auto section_client = GetSection(ini, L"client");
	Parse(section_client, L"batchfile", client_batchfile_);
	Parse(section_client, L"executable", client_executable_);
	std::vector<std::wstring> args(10);
	Parse(section_client, L"args0", args[0]);
	Parse(section_client, L"args1", args[1]);
	Parse(section_client, L"args2", args[2]);
	Parse(section_client, L"args3", args[3]);
	Parse(section_client, L"args4", args[4]);
	Parse(section_client, L"args5", args[5]);
	Parse(section_client, L"args6", args[6]);
	Parse(section_client, L"args7", args[7]);
	Parse(section_client, L"args8", args[8]);
	Parse(section_client, L"args9", args[9]);
	std::wostringstream all_args;
	std::copy(args.begin(), args.end(), std::ostream_iterator<std::wstring, wchar_t>(all_args, L" "));
	client_args_ = all_args.str();
	LOG->debug("all args = {}", wstring_to_utf8(client_args_));
	audioformats_ = GetSection(ini, L"audioformats");
	videoformats_ = GetSection(ini, L"videoformats");
	LOG_EXIT;
}
