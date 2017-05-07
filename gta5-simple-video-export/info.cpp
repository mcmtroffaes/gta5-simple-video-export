/*
Manage information during the export process.

Info is created at the very start of the export, right after the ini file is
reloaded. It just stores the timestamp and the game folder.

AudioInfo and VideoInfo are created when the audio and video input formats are
set. They store all relevant information about the format. They also store the
handles to the files (or pipes) for writing the actual raw uncompressed data.
*/

#include "info.h"

#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>

#include <Shlwapi.h> // PathCombine
#pragma comment(lib, "Shlwapi.lib")

auto MakePath(const std::string & part1, const std::string & part2) {
	LOG_ENTER;
	char path[MAX_PATH] = "";
	if (PathCombineA(path, part1.c_str(), part2.c_str()) == nullptr) {
		LOG->error("could not combine {} and {} to form path of output stream", part1, part2);
	}
	LOG_EXIT;
	return std::string(path);
}

template <typename T>
std::string ToString(const T & value) {
	std::ostringstream oss;
	oss << value;
	return oss.str();
}

std::string GUIDToString(const GUID & guid) {
	LOG_ENTER;
	char buffer[48];
	snprintf(buffer, sizeof(buffer), "%08lX-%04hX-%04hX-%02hhX%02hhX-%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX",
		guid.Data1, guid.Data2, guid.Data3,
		guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
		guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);
	LOG_EXIT;
	return std::string(buffer);
}

void StringReplace(std::string & str, const std::string & old_str, const std::string & new_str)
{
	LOG_ENTER;
	// empty string represents an uninitialized value
	if (!new_str.empty()) {
		std::string::size_type pos = 0;
		while ((pos = str.find(old_str, pos)) != std::string::npos) {
			// we have a lot of these messages; so use trace instead of debug
			LOG->trace("replacing \"{}\" by \"{}\" in \"{}\"", old_str, new_str, str);
			str.replace(pos, old_str.length(), new_str);
			pos += new_str.length();
		}
	}
	else if (str.find(old_str) != std::string::npos) {
		LOG->error("cannot replace \"{}\" in \"{}\" as its value is not yet identified", old_str, str);
	}
	LOG_EXIT;
}

void StringReplace(std::string & str, const std::string & old_str, uint32_t new_value) {
	LOG_ENTER;
	// max represents an uninitialized value
	if (new_value != UINT32_MAX) {
		StringReplace(str, old_str, ToString(new_value));
	}
	else {
		StringReplace(str, old_str, "");
	}
	LOG_EXIT;
}

std::string ScriptFolder()
{
	LOG_ENTER;
	char buffer[MAX_PATH];
	GetModuleFileNameA(NULL, buffer, MAX_PATH);
	std::string::size_type pos = std::string(buffer).find_last_of("\\/");
	auto path = MakePath(std::string(buffer).substr(0, pos), SCRIPT_FOLDER);
	LOG_EXIT;
	return path;
}

std::string TimeStamp()
{
	LOG_ENTER;
	time_t rawtime;
	struct tm timeinfo;
	time(&rawtime);
	localtime_s(&timeinfo, &rawtime);
	std::ostringstream oss;
	oss << std::put_time(&timeinfo, "%Y%d%m-%H%M%S");
	LOG_EXIT;
	return oss.str();
}

GeneralInfo::GeneralInfo()
	: scriptfolder_(ScriptFolder())
	, timestamp_(TimeStamp())
{
}

void GeneralInfo::Substitute(std::string & str) const {
	LOG_ENTER;
	StringReplace(str, "${scriptfolder}", scriptfolder_);
	StringReplace(str, "${timestamp}", timestamp_);
	LOG_EXIT;
};

AudioInfo::AudioInfo(DWORD stream_index, IMFMediaType & input_media_type, const Settings & settings, const GeneralInfo & info)
	: audio_format_()
	, audio_path_()
	, audio_rate_(UINT32_MAX)
	, audio_num_channels_(UINT32_MAX)
	, audio_bits_per_sample_(UINT32_MAX)
	, stream_index_(stream_index)
	, os_(nullptr)
{
	LOG_ENTER;
	stream_index_ = stream_index;
	LOG->debug("audio stream index = {}", stream_index_);
	auto folder = settings.raw_folder_;
	auto filename = settings.raw_audio_filename_;
	info.Substitute(folder);
	info.Substitute(filename);
	audio_path_ = MakePath(folder, filename);
	LOG->debug("audio path = {}", audio_path_);
	GUID subtype = { 0 };
	auto hr = input_media_type.GetGUID(MF_MT_SUBTYPE, &subtype);
	if (SUCCEEDED(hr)) {
		auto guid_str = GUIDToString(subtype);
		auto format = settings.videoformats_.find(guid_str);
		if (format != settings.videoformats_.end()) {
			audio_format_ = format->second;
			LOG->info("audio format = {} = {}", guid_str, audio_format_);
			hr = input_media_type.GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &audio_rate_);
		}
		else {
			LOG->error("audio format = {} = unsupported", guid_str);
			LOG->error("please add an entry for audio format {} in {}", guid_str, settings.ini_filename_);
			hr = E_FAIL;
		}
	}
	if (SUCCEEDED(hr)) {
		LOG->info("audio rate = {}", audio_rate_);
		hr = input_media_type.GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, &audio_num_channels_);
	}
	if (SUCCEEDED(hr)) {
		LOG->info("audio num channels = {}", audio_num_channels_);
		hr = input_media_type.GetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, &audio_bits_per_sample_);
	}
	if (SUCCEEDED(hr)) {
		LOG->info("audio bits per sample = {}", audio_bits_per_sample_);
		os_.reset(new FileHandle(audio_path_, settings.IsRawFolderPipe()));
	}
	LOG_EXIT;
}

void AudioInfo::Substitute(std::string & str) const {
	StringReplace(str, "${audio_format}", audio_format_);
	StringReplace(str, "${audio_path}", audio_path_);
	StringReplace(str, "${audio_rate}", audio_rate_);
	StringReplace(str, "${audio_num_channels}", audio_num_channels_);
	StringReplace(str, "${audio_bits_per_sample}", audio_bits_per_sample_);
}

VideoInfo::VideoInfo(DWORD stream_index, IMFMediaType & input_media_type, const Settings & settings, const GeneralInfo & info)
	: video_format_()
	, video_path_()
	, width_(UINT32_MAX)
	, height_(UINT32_MAX)
	, framerate_numerator_(UINT32_MAX)
	, framerate_denominator_(UINT32_MAX)
	, stream_index_(stream_index)
	, os_(nullptr)
{
	LOG_ENTER;
	LOG->debug("video stream index = {}", stream_index_);
	auto folder = settings.raw_folder_;
	auto filename = settings.raw_video_filename_;
	info.Substitute(folder);
	info.Substitute(filename);
	video_path_ = MakePath(folder, filename);
	LOG->debug("video path = {}", video_path_);
	GUID subtype = { 0 };
	auto hr = input_media_type.GetGUID(MF_MT_SUBTYPE, &subtype);
	if (SUCCEEDED(hr)) {
		auto guid_str = GUIDToString(subtype);
		auto format = settings.videoformats_.find(guid_str);
		if (format != settings.videoformats_.end()) {
			video_format_ = format->second;
			LOG->info("video format = {} = {}", guid_str, video_format_);
			hr = MFGetAttributeSize(&input_media_type, MF_MT_FRAME_SIZE, &width_, &height_);
		}
		else {
			LOG->error("video format = {} = unsupported", guid_str);
			LOG->error("please add an entry for video format {} in {}", guid_str, settings.ini_filename_);
			hr = E_FAIL;
		}
	}
	if (SUCCEEDED(hr)) {
		LOG->info("video size = {}x{}", width_, height_);
		hr = MFGetAttributeRatio(&input_media_type, MF_MT_FRAME_RATE, &framerate_numerator_, &framerate_denominator_);
	}
	if (SUCCEEDED(hr)) {
		LOG->info("video framerate = {}/{}", framerate_numerator_, framerate_denominator_);
		os_.reset(new FileHandle(video_path_, settings.IsRawFolderPipe()));
	}
	LOG_EXIT;
}

void VideoInfo::Substitute(std::string & str) const {
	StringReplace(str, "${video_format}", video_format_);
	StringReplace(str, "${video_path}", video_path_);
	StringReplace(str, "${width}", width_);
	StringReplace(str, "${height}", height_);
	StringReplace(str, "${framerate_numerator}", framerate_numerator_);
	StringReplace(str, "${framerate_denominator}", framerate_denominator_);
}

void CreateClientBatchFile(const Settings & settings, const GeneralInfo & info, const AudioInfo & audio_info, const VideoInfo & video_info)
{
	LOG_ENTER;
	auto executable = settings.client_executable_;
	info.Substitute(executable);
	auto args = settings.client_args_;
	info.Substitute(args);
	audio_info.Substitute(args);
	video_info.Substitute(args);
	auto batchfile = settings.client_batchfile_;
	info.Substitute(batchfile);
	audio_info.Substitute(batchfile);
	video_info.Substitute(batchfile);
	LOG->info("creating {}; run this file to process the raw output", batchfile);
	std::ofstream os(settings.client_batchfile_, std::ios::out | std::ios::trunc);
	os << "@echo off" << std::endl;
	os << '"' << executable << '"' << ' ' << args << std::endl;
	os << "pause" << std::endl;
	os.close();
	LOG_EXIT;
}
