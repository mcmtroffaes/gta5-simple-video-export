#include "info.h"

#include <iomanip>
#include <ctime>
#include <sstream>

#include <Shlwapi.h> // PathCombine
#pragma comment(lib, "Shlwapi.lib")

template <typename T>
std::string ToString(const T & value) {
	std::ostringstream oss;
	oss << value;
	return oss.str();
}

std::string GUIDToString(const GUID & guid) {
	char buffer[48];
	snprintf(buffer, sizeof(buffer), "%08lX-%04hX-%04hX-%02hhX%02hhX-%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX",
		guid.Data1, guid.Data2, guid.Data3,
		guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
		guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);
	return std::string(buffer);
}

void StringReplace(std::string & str, const std::string & old_str, const std::string & new_str)
{
	LOG_ENTER;
	// empty string represents an uninitialized value
	if (!new_str.empty()) {
		std::string::size_type pos = 0;
		while ((pos = str.find(old_str, pos)) != std::string::npos) {
			LOG->debug("replacing \"{}\" by \"{}\" in \"{}\"", old_str, new_str, str);
			str.replace(pos, old_str.length(), new_str);
			pos += new_str.length();
		}
	}
	else if (str.find(old_str, 0) != std::string::npos) {
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

std::string GameFolder()
{
	char buffer[MAX_PATH];
	GetModuleFileNameA(NULL, buffer, MAX_PATH);
	std::string::size_type pos = std::string(buffer).find_last_of("\\/");
	return std::string(buffer).substr(0, pos);
}

std::string TimeStamp()
{
	time_t rawtime;
	struct tm timeinfo;
	time(&rawtime);
	localtime_s(&timeinfo, &rawtime);
	std::ostringstream oss;
	oss << std::put_time(&timeinfo, "%Y%d%m%H%M%S");
	return oss.str();
}

auto MakePath(const std::string & folder, const std::string & filename) {
	char path[MAX_PATH] = "";
	if (PathCombineA(path, folder.c_str(), filename.c_str()) == nullptr) {
		LOG->error("could not combine {} and {} to form path of output stream", folder, filename);
	}
	return std::string(path);
}

GeneralInfo::GeneralInfo()
	: gamefolder_(GameFolder())
	, timestamp_(TimeStamp())
{
}

void GeneralInfo::Substitute(std::string & str) const {
	StringReplace(str, "${gamefolder}", gamefolder_);
	StringReplace(str, "${timestamp}", timestamp_);
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
		if (subtype == MFAudioFormat_PCM) {
			audio_format_ = "s16le";
			LOG->info("audio format = PCM {}", GUIDToString(subtype));
			hr = input_media_type.GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &audio_rate_);
		}
		else {
			LOG->error("audio format unsupported");
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
		if (subtype == MFVideoFormat_NV12) {
			video_format_ = "nv12";
			LOG->info("video format = NV12 {}", GUIDToString(subtype));
			hr = MFGetAttributeSize(&input_media_type, MF_MT_FRAME_SIZE, &width_, &height_);
		}
		else {
			LOG->error("video format unsupported");
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
