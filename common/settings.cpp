#include "settings.h"
#include "../spdlog/include/spdlog/sinks/rotating_file_sink.h"

#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>

#include <ShlObj.h> // SHGetKnownFolderPath

std::wstring GetKnownFolder(const KNOWNFOLDERID & fldrid)
{
	LOG_ENTER;
	PWSTR path = NULL;
	auto hr = SHGetKnownFolderPath(fldrid, 0, NULL, &path);
	if (SUCCEEDED(hr)) {
		auto path2 = std::wstring(path);
		CoTaskMemFree(path);
		LOG_EXIT;
		return path2;
	}
	else {
		LOG->error("failed to get known folder");
		LOG_EXIT;
		return std::wstring();
	}
}

std::wstring TimeStamp()
{
	LOG_ENTER;
	time_t rawtime;
	struct tm timeinfo;
	time(&rawtime);
	localtime_s(&timeinfo, &rawtime);
	std::wostringstream oss;
	oss << std::put_time(&timeinfo, L"%Y%d%m-%H%M%S");
	LOG_EXIT;
	return oss.str();
}

const std::wstring Settings::ini_filename_ = SCRIPT_NAME L".ini";

Settings::Settings()
{
	LOG_ENTER;
	LOG->debug("parsing {}", wstring_to_utf8(ini_filename_));
	std::wifstream is(ini_filename_);
	if (is.fail()) {
		LOG->error("failed to open \"{}\"", wstring_to_utf8(ini_filename_));
	}
	else {
		parse(is);
		if (!errors.empty()) {
			for (auto error : errors) {
				LOG->error("failed to parse \"{}\"", wstring_to_utf8(error));
			}
		}
	}
	Section & secdef = sections[L"builtin"];
	auto timestamp = TimeStamp();
	auto docs = GetKnownFolder(FOLDERID_Documents);
	auto vids = GetKnownFolder(FOLDERID_Videos);
	auto desk = GetKnownFolder(FOLDERID_Desktop);
	secdef[L"timestamp"] = timestamp;
	secdef[L"documentsfolder"] = docs;
	secdef[L"videosfolder"] = vids;
	secdef[L"desktopfolder"] = desk;
	LOG->debug("timestamp = {}", wstring_to_utf8(timestamp));
	LOG->debug("documentsfolder = {}", wstring_to_utf8(docs));
	LOG->debug("videosfolder = {}", wstring_to_utf8(vids));
	LOG->debug("desktopfolder = {}", wstring_to_utf8(desk));
	LOG_EXIT;
}

const Settings::Section & Settings::GetSec(const std::wstring & sec_name) const {
	LOG_ENTER;
	auto sec = sections.find(sec_name);
	if (sec != sections.end()) {
		LOG_EXIT;
		return sec->second;
	}
	else {
		LOG->error("section [{}] not found", wstring_to_utf8(sec_name));
		LOG_EXIT;
		return Settings::Section();
	}
}

void Settings::ResetLogger() {
	LOG_ENTER;
	auto level{ spdlog::level::info };
	auto flush_on{ spdlog::level::off };
	auto sec = GetSec(L"log");
	GetVar(sec, L"level", level);
	GetVar(sec, L"flush_on", flush_on);
	logger->flush();
	logger->set_level(level);
	logger->flush_on(flush_on);
	LOG_EXIT;
}
