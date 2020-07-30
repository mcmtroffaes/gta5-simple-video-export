#include "settings.h"

#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>

extern "C" {
#include <libavformat/avformat.h>
}

#ifdef _WIN32
#include <ShlObj.h> // SHGetKnownFolderPath

auto GetKnownFolder(const KNOWNFOLDERID & fldrid)
{
	LOG_ENTER;
	PWSTR path = NULL;
	auto hr = SHGetKnownFolderPath(fldrid, 0, NULL, &path);
	if (SUCCEEDED(hr)) {
		std::filesystem::path path2{ path };
		CoTaskMemFree(path);
		LOG_EXIT;
		return path2;
	}
	else {
		LOG->error("failed to get known folder");
		LOG_EXIT;
		return std::filesystem::path{};
	}
}
#endif

std::string TimeStamp()
{
	LOG_ENTER;
	time_t rawtime;
	struct tm timeinfo{};
	time(&rawtime);
#ifdef _WIN32
	localtime_s(&timeinfo, &rawtime);
#else
	localtime_r(&rawtime, &timeinfo);
#endif
	std::ostringstream oss;
	oss << std::put_time(&timeinfo, "%Y%d%m-%H%M%S");
	LOG_EXIT;
	return oss.str();
}

void ParseCodecNameOptions(const std::string& value, std::string& name, AVDictionaryPtr& options) {
	LOG_ENTER;
	auto pos = value.find_first_of(' ');
	if (pos == std::string::npos) {
		name = value;
		options = nullptr;
		LOG->debug("codec name is {}", name);
	}
	else {
		name = value.substr(0, pos);
		std::string options_string{ value.substr(pos + 1) };
		options = CreateAVDictionary(options_string, ":", ",");
		LOG->debug("codec name is {}", name);
		LOG->debug("codec options are {}", options_string);
	}
	LOG_EXIT;
}

const std::filesystem::path Settings::ini_filename_ = SCRIPT_NAME ".ini";

Settings::Settings()
	: video_codec_id{ AV_CODEC_ID_NONE }
	, audio_codec_id{ AV_CODEC_ID_NONE }
{
	// LOG_ENTER is deferred until the log level is set
	LOG->debug("parsing {}", ini_filename_.string());
	std::ifstream is(ini_filename_);
	if (is.fail()) {
		LOG->error("failed to open \"{}\"", ini_filename_.string());
	}
	else {
		parse(is);
		if (!errors.empty()) {
			for (const auto& error : errors) {
				LOG->error("failed to parse \"{}\"", error);
			}
		}
	}
	auto level{ spdlog::level::info };
	auto flush_on{ spdlog::level::off };
	auto sec = GetSec(sections, "log");
	GetVar(sec, "level", level);
	GetVar(sec, "flush_on", flush_on);
	if (logger) {
		logger->flush();
		logger->set_level(level);
		logger->flush_on(flush_on);
	}
	AVLogSetLevel(level);
	PLHLogSetLevel(level);
	LOG_ENTER_METHOD;
	Section & builtinsec = sections["builtin"];
	auto timestamp = TimeStamp();
	builtinsec["timestamp"] = timestamp;
	LOG->debug("timestamp = {}", timestamp);
#ifdef _WIN32
	auto docs = GetKnownFolder(FOLDERID_Documents).u8string();
	auto vids = GetKnownFolder(FOLDERID_Videos).u8string();
	auto desk = GetKnownFolder(FOLDERID_Desktop).u8string();
	builtinsec["documentsfolder"] = docs;
	builtinsec["videosfolder"] = vids;
	builtinsec["desktopfolder"] = desk;
	LOG->debug("documentsfolder = {}", docs);
	LOG->debug("videosfolder = {}", vids);
	LOG->debug("desktopfolder = {}", desk);
#endif
	// interpolate all variables
	interpolate();
	// set up a valid filename
	auto exportsec = GetSec(sections, "export");
	std::string folder{ "." };
	std::string basename{ "sve-" + timestamp };
	std::string preset{ };
	GetVar(exportsec, "folder", folder);
	GetVar(exportsec, "basename", basename);
	GetVar(exportsec, "preset", preset);
	auto presetsec = GetSec(sections, preset);
	std::string container{ "mkv" };
	GetVar(presetsec, "container", container);
	export_filename = folder;
	export_filename /= basename + "." + container;
	auto u8_export_filename{ export_filename.u8string() };
	auto c_export_filename{ reinterpret_cast<const char*>(u8_export_filename.c_str()) };
	auto oformat = av_guess_format(nullptr, c_export_filename, nullptr);
	if (!oformat) {
		LOG->error("container format {} not supported, falling back to mkv", container);
		export_filename = folder;
		export_filename /= basename + ".mkv";
		u8_export_filename = export_filename.u8string();
		c_export_filename = reinterpret_cast<const char*>(u8_export_filename.c_str());
		oformat = av_guess_format(nullptr, c_export_filename, nullptr);
		if (!oformat)
			throw std::runtime_error("mkv container format not supported");
	}
	// set up valid video codec
	std::string videocodec_value{ };
	std::string videocodec_name{ };
	GetVar(presetsec, "videocodec", videocodec_value);
	ParseCodecNameOptions(videocodec_value, videocodec_name, video_codec_options);
	auto videocodec = avcodec_find_encoder_by_name(videocodec_name.c_str());
	auto videocodec_fallback = oformat->video_codec;
	if (!videocodec)
		LOG->error(
			"video codec {} not supported, falling back to {}",
			videocodec_name, avcodec_get_name(videocodec_fallback));
	video_codec_id = videocodec ? videocodec->id : videocodec_fallback;
	// set up valid audio codec
	std::string audiocodec_value{ };
	std::string audiocodec_name{ };
	GetVar(presetsec, "audiocodec", audiocodec_value);
	ParseCodecNameOptions(audiocodec_value, audiocodec_name, audio_codec_options);
	auto audiocodec = avcodec_find_encoder_by_name(audiocodec_name.c_str());
	auto audiocodec_fallback = oformat->audio_codec;
	if (!audiocodec)
		LOG->error(
			"audio codec {} not supported, falling back to {}",
			audiocodec_name, avcodec_get_name(audiocodec_fallback));
	audio_codec_id = audiocodec ? audiocodec->id : audiocodec_fallback;
	LOG_EXIT_METHOD;
}

const Settings::Section empty_section{};

const inipp::Ini<char>::Section& GetSec(const inipp::Ini<char>::Sections& sections, const std::string & sec_name) {
	LOG_ENTER;
	auto sec = sections.find(sec_name);
	if (sec == sections.end()) {
		LOG->error("section [{}] not found", sec_name);
		LOG_EXIT;
		return empty_section;
	}
	LOG_EXIT;
	return sec->second;
}
