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

std::string wstring_to_utf8(const std::wstring& str)
{
	std::wstring_convert<std::codecvt_utf8<wchar_t> > myconv;
	return myconv.to_bytes(str);
}

std::string GetKnownFolder(const KNOWNFOLDERID & fldrid)
{
	LOG_ENTER;
	PWSTR path = NULL;
	auto hr = SHGetKnownFolderPath(fldrid, 0, NULL, &path);
	if (SUCCEEDED(hr)) {
		auto path2 = wstring_to_utf8(std::wstring(path));
		CoTaskMemFree(path);
		LOG_EXIT;
		return path2;
	}
	else {
		LOG->error("failed to get known folder");
		LOG_EXIT;
		return std::string();
	}
}
#endif

std::string TimeStamp()
{
	LOG_ENTER;
	time_t rawtime;
	struct tm timeinfo;
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

const std::filesystem::path Settings::ini_filename_ = SCRIPT_NAME ".ini";

Settings::Settings()
	: export_filename{}
	, video_codec_id{ AV_CODEC_ID_NONE }
	, audio_codec_id{ AV_CODEC_ID_NONE }
{
	// LOG_ENTER is deferred until the log level is set
	LOG->debug("parsing {}", ini_filename_.u8string());
	std::ifstream is(ini_filename_);
	if (is.fail()) {
		LOG->error("failed to open \"{}\"", ini_filename_.u8string());
	}
	else {
		parse(is);
		if (!errors.empty()) {
			for (auto error : errors) {
				LOG->error("failed to parse \"{}\"", error);
			}
		}
	}
	auto level{ spdlog::level::info };
	auto flush_on{ spdlog::level::off };
	auto sec = GetSec("log");
	GetVar(sec, "level", level);
	GetVar(sec, "flush_on", flush_on);
	if (logger) {
		logger->flush();
		logger->set_level(level);
		logger->flush_on(flush_on);
	}
	AVLogSetLevel(level);
	LOG_ENTER;
	Section & builtinsec = sections["builtin"];
	auto timestamp = TimeStamp();
	builtinsec["timestamp"] = timestamp;
	LOG->debug("timestamp = {}", timestamp);
#ifdef _WIN32
	auto docs = GetKnownFolder(FOLDERID_Documents);
	auto vids = GetKnownFolder(FOLDERID_Videos);
	auto desk = GetKnownFolder(FOLDERID_Desktop);
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
	auto exportsec = GetSec("export");
	std::string folder{ "." };
	std::string basename{ "sve-" + timestamp };
	std::string preset{ };
	GetVar(exportsec, "folder", folder);
	GetVar(exportsec, "basename", basename);
	GetVar(exportsec, "preset", preset);
	auto presetsec = GetSec(preset);
	std::string container{ "mp4" };
	GetVar(presetsec, "container", container);
	export_filename = folder;
	export_filename /= basename + "." + container;
	auto oformat = av_guess_format(nullptr, export_filename.u8string().c_str(), nullptr);
	if (!oformat) {
		LOG->error("container format {} not supported, falling back to mp4", container);
		export_filename = folder;
		export_filename /= basename + ".mp4";
		oformat = av_guess_format(nullptr, export_filename.u8string().c_str(), nullptr);
		if (!oformat)
			LOG_THROW(std::runtime_error, "mp4 output format not supported");
	}
	// set up valid video codec
	std::string videocodec_name{ };
	GetVar(presetsec, "videocodec", videocodec_name);
	auto videocodec = avcodec_find_encoder_by_name(videocodec_name.c_str());
	auto videocodec_fallback = oformat->video_codec;
	if (!videocodec)
		LOG->error(
			"video codec {} not supported, falling back to {}",
			videocodec_name, avcodec_get_name(videocodec_fallback));
	video_codec_id = videocodec ? videocodec->id : videocodec_fallback;
	// set up valid audio codec
	std::string audiocodec_name{ };
	GetVar(presetsec, "audiocodec", audiocodec_name);
	auto audiocodec = avcodec_find_encoder_by_name(audiocodec_name.c_str());
	auto audiocodec_fallback = oformat->audio_codec;
	if (!audiocodec)
		LOG->error(
			"audio codec {} not supported, falling back to {}",
			audiocodec_name, avcodec_get_name(audiocodec_fallback));
	audio_codec_id = audiocodec ? audiocodec->id : audiocodec_fallback;
	LOG_EXIT;
}

const Settings::Section empty_section{};

const Settings::Section & Settings::GetSec(const std::string & sec_name) const {
	LOG_ENTER;
	auto sec = sections.find(sec_name);
	if (sec != sections.end()) {
		LOG_EXIT;
		return sec->second;
	}
	else {
		LOG->error("section [{}] not found", sec_name);
		LOG_EXIT;
		return empty_section;
	}
}
