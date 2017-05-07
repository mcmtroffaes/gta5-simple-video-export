#include "settings.h"
#include "logger.h"
#include "../ini-parser/ini.hpp"

bool Convert(const std::string & value_str, std::string & value)
{
	value = value_str;
	return true;
}

bool Convert(const std::string & value_str, bool & value) {
	if (value_str == "true") {
		value = true;
		return true;
	}
	else if (value_str == "false") {
		value = false;
		return true;
	}
	else {
		return false;
	}
}

bool Convert(const std::string & value_str, spdlog::level::level_enum & value) {
	if (value_str == "trace") {
		value = spdlog::level::trace;
		return true;
	}
	if (value_str == "debug") {
		value = spdlog::level::debug;
		return true;
	}
	if (value_str == "info") {
		value = spdlog::level::info;
		return true;
	}
	if (value_str == "warn") {
		value = spdlog::level::warn;
		return true;
	}
	if (value_str == "err") {
		value = spdlog::level::err;
		return true;
	}
	if (value_str == "critical") {
		value = spdlog::level::critical;
		return true;
	}
	if (value_str == "off") {
		value = spdlog::level::off;
		return true;
	}
	return false;
}

template <typename T>
bool Parse(const INI::Level *level, const std::string & name, T & value)
{
	LOG_ENTER;
	auto success = false;
	if (!level) {
		LOG->debug("value {} not found", name);
	}
	else {
		auto keyvalue = level->values.find(name);
		if (keyvalue == level->values.end()) {
			LOG->debug("value {} not found", name);
		}
		else {
			auto value_str = keyvalue->second;
			success = Convert(value_str, value);
			if (success) {
				LOG->debug("{} = {}", name, value_str);
			}
			else {
				LOG->error("{} = parse error ({})", name, value_str);
			}
		}
	}
	LOG_EXIT;
	return success;
}

const INI::Level *GetSection(const INI::Level & parent, const std::string & name) {
	LOG_ENTER;
	auto keyvalue = parent.sections.find(name);
	if (keyvalue != parent.sections.end()) {
		LOG_EXIT;
		return &keyvalue->second;
	}
	else {
		LOG->error("section {} not found", name);
		LOG_EXIT;
		return nullptr;
	}
}

BOOL DirectoryExists(LPCSTR szPath)
{
	DWORD dwAttrib = GetFileAttributesA(szPath);
	return (dwAttrib != INVALID_FILE_ATTRIBUTES && (dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

const std::string Settings::ini_filename_ = SCRIPT_FOLDER "\\config.ini";

Settings::Settings()
// note: default settings here must match the default config.ini that is shipped
	: enable_(true)
	, log_level_(spdlog::level::info)
	, log_flush_on_(spdlog::level::off)
	, raw_folder_("${scriptfolder}") // TODO: default to "\\\\.\\pipe\\" instead
	, raw_video_filename_("sve-${timestamp}-video.yuv")
	, raw_audio_filename_("sve-${timestamp}-audio.raw")
	, client_batchfile_("${scriptfolder}\sve-${timestamp}-client.bat")
	, client_executable_("${scriptfolder}\\ffmpeg.exe")
	, client_args_() // TODO: set up default
	, videoformats_()
	, audioformats_()
{
	LOG_ENTER;
	std::unique_ptr<INI::Parser> parser = nullptr;
	try {
		LOG->debug("parsing {}", ini_filename_);
		parser.reset(new INI::Parser(ini_filename_.c_str()));
	}
	catch (const std::exception & ex) {
		LOG->error("{} parse error ({})", ini_filename_, ex.what());
	}
	catch (...) {
		LOG->error("{} unknown parse error", ini_filename_);
	}
	if (parser) {
		// always parse the [log] section first (even if mod is disabled)
		auto section_log = GetSection(parser->top(), "log");
		Parse(section_log, "level", log_level_);
		Parse(section_log, "flush_on", log_flush_on_);
		LOG->set_level(settings->log_level_);
		LOG->flush_on(settings->log_flush_on_);
		// now parse everything else
		Parse(&parser->top(), "enable", enable_);
		auto section_raw = GetSection(parser->top(), "raw");
		if (Parse(section_raw, "folder", raw_folder_)) {
			if (raw_folder_ != "\\\\.\\pipe\\" && !DirectoryExists(raw_folder_.c_str())) {
				LOG->error("folder {} does not exist; using " SCRIPT_FOLDER, raw_folder_);
				raw_folder_ = SCRIPT_FOLDER;
			}
		}
		Parse(section_raw, "video_filename", raw_video_filename_);
		Parse(section_raw, "audio_filename", raw_audio_filename_);
		auto section_client = GetSection(parser->top(), "client");
		Parse(section_client, "batchfile", client_batchfile_);
		Parse(section_client, "executable", client_executable_);
		std::vector<std::string> args(10);
		Parse(section_client, "args0", args[0]);
		Parse(section_client, "args1", args[1]);
		Parse(section_client, "args2", args[2]);
		Parse(section_client, "args3", args[3]);
		Parse(section_client, "args4", args[4]);
		Parse(section_client, "args5", args[5]);
		Parse(section_client, "args6", args[6]);
		Parse(section_client, "args7", args[7]);
		Parse(section_client, "args8", args[8]);
		Parse(section_client, "args9", args[9]);
		std::ostringstream all_args;
		std::copy(args.begin(), args.end(), std::ostream_iterator<std::string>(all_args, " "));
		client_args_ = all_args.str();
		LOG->debug("all args = {}", client_args_);
		auto section_audioformats = GetSection(parser->top(), "audioformats");
		if (section_audioformats) {
			audioformats_ = section_audioformats->values;
		}
		auto section_videoformats = GetSection(parser->top(), "videoformats");
		if (section_videoformats) {
			videoformats_ = section_videoformats->values;
		}
	}
	LOG_EXIT;
}
