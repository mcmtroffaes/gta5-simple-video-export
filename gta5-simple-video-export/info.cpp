#include "info.h"

#include <fstream>
#include <sstream>

auto GUIDToString(const GUID & guid) {
	LOG_ENTER;
	char buffer[48];
	_snprintf_s(buffer, sizeof(buffer), "%08lX-%04hX-%04hX-%02hhX%02hhX-%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX",
		guid.Data1, guid.Data2, guid.Data3,
		guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
		guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);
	LOG_EXIT;
	return std::string(buffer);
}

AudioInfo::AudioInfo(DWORD stream_index, IMFMediaType & input_media_type)
	: subtype_{ 0 }
	, rate_(UINT32_MAX)
	, num_channels_(UINT32_MAX)
	, bits_per_sample_(UINT32_MAX)
	, stream_index_(stream_index)
	, handle_(nullptr)
{
	LOG_ENTER;
	LOG->debug("audio stream index = {}", stream_index_);
	auto hr = input_media_type.GetGUID(MF_MT_SUBTYPE, &subtype_);
	if (SUCCEEDED(hr)) {
		LOG->info("audio subtype = {}", GUIDToString(subtype_));
		hr = input_media_type.GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &rate_);
	}
	if (SUCCEEDED(hr)) {
		LOG->info("audio rate = {}", rate_);
		hr = input_media_type.GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, &num_channels_);
	}
	if (SUCCEEDED(hr)) {
		LOG->info("audio num channels = {}", num_channels_);
		hr = input_media_type.GetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, &bits_per_sample_);
	}
	if (SUCCEEDED(hr)) {
		LOG->info("audio bits per sample = {}", bits_per_sample_);
	}
	LOG_EXIT;
}

void AudioInfo::UpdateSettings(Settings & settings) const {
	LOG_ENTER;
	auto & defsec = settings.sections["builtin"];
	defsec["audio_rate"] = std::to_string(rate_);
	defsec["audio_num_channels"] = std::to_string(num_channels_);
	defsec["audio_bits_per_sample"] = std::to_string(bits_per_sample_);
	const auto & audioformats = settings.GetSec("audioformats");
	auto subtype_str = GUIDToString(subtype_);
	auto format = audioformats.find(subtype_str);
	if (format != audioformats.end()) {
		defsec["audio_format"] = format->second;
		LOG->info("audio format = {}", format->second);
	}
	else {
		LOG->error("audio subtype = {} = unsupported", subtype_str);
		LOG->error("please add an audioformats entry for {} in {}", subtype_str, settings.ini_filename_);
	}
	LOG_EXIT;
}

VideoInfo::VideoInfo(DWORD stream_index, IMFMediaType & input_media_type)
	: subtype_{ 0 }
	, width_(UINT32_MAX)
	, height_(UINT32_MAX)
	, framerate_numerator_(UINT32_MAX)
	, framerate_denominator_(UINT32_MAX)
	, stream_index_(stream_index)
	, handle_(nullptr)
{
	LOG_ENTER;
	LOG->debug("video stream index = {}", stream_index_);
	auto hr = input_media_type.GetGUID(MF_MT_SUBTYPE, &subtype_);
	if (SUCCEEDED(hr)) {
		LOG->info("video subtype = {}", GUIDToString(subtype_));
		hr = MFGetAttributeSize(&input_media_type, MF_MT_FRAME_SIZE, &width_, &height_);
	}
	if (SUCCEEDED(hr)) {
		LOG->info("video size = {}x{}", width_, height_);
		hr = MFGetAttributeRatio(&input_media_type, MF_MT_FRAME_RATE, &framerate_numerator_, &framerate_denominator_);
	}
	if (SUCCEEDED(hr)) {
		LOG->info("video framerate = {}/{}", framerate_numerator_, framerate_denominator_);
	}
	LOG_EXIT;
}

void VideoInfo::UpdateSettings(Settings & settings) const {
	LOG_ENTER;
	auto & defsec = settings.sections["builtin"];
	defsec["width"] = std::to_string(width_);
	defsec["height"] = std::to_string(height_);
	defsec["framerate_numerator"] = std::to_string(framerate_numerator_);
	defsec["framerate_denominator"] = std::to_string(framerate_denominator_);
	const auto & videoformats = settings.GetSec("videoformats");
	auto subtype_str = GUIDToString(subtype_);
	auto format = videoformats.find(subtype_str);
	if (format != videoformats.end()) {
		defsec["video_format"] = format->second;
		LOG->info("video format = {}", format->second);
	}
	else {
		LOG->error("video subtype = {} = unsupported", subtype_str);
		LOG->error("please add a videoformats entry for {} in {}", subtype_str, settings.ini_filename_);
	}
	LOG_EXIT;
}

void CreateBatchFile(const Settings & settings)
{
	LOG_ENTER;
	std::wstring batch_filename;
	auto sec = settings.GetSec("raw");
	if (settings.GetVar(sec, "batch_filename", batch_filename)) {
		std::string batch_command;
		if (settings.GetVar(sec, "batch_command", batch_command)) {
			LOG->info("creating {}; run this file to process the raw output", batch_filename);
			// TODO turn batch_filename into a UTF16 encoded wstring
			std::ofstream os(batch_filename, std::ios::out | std::ios::trunc);
			os << "@echo off" << std::endl;
			os << batch_command << std::endl;
			os << "pause" << std::endl;
			os.close();
		}
		else {
			LOG->error("batch_command not set; no batch file will be created");
		}
	}
	else {
		LOG->error("batch_filename not set; no batch file will be created");
	}
	LOG_EXIT;
}
