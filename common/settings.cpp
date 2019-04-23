#include "settings.h"

#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>

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

const std::string Settings::ini_filename_ = SCRIPT_NAME ".ini";

Settings::Settings()
{
	// LOG_ENTER is deferred until the log level is set
	LOG->debug("parsing {}", ini_filename_);
	std::ifstream is(ini_filename_);
	if (is.fail()) {
		LOG->error("failed to open \"{}\"", ini_filename_);
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
	Section & secdef = sections["builtin"];
	auto timestamp = TimeStamp();
	secdef["timestamp"] = timestamp;
	LOG->debug("timestamp = {}", timestamp);
#ifdef _WIN32
	auto docs = GetKnownFolder(FOLDERID_Documents);
	auto vids = GetKnownFolder(FOLDERID_Videos);
	auto desk = GetKnownFolder(FOLDERID_Desktop);
	secdef["documentsfolder"] = docs;
	secdef["videosfolder"] = vids;
	secdef["desktopfolder"] = desk;
	LOG->debug("documentsfolder = {}", docs);
	LOG->debug("videosfolder = {}", vids);
	LOG->debug("desktopfolder = {}", desk);
#endif
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
