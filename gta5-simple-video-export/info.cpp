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

VideoInfo::VideoInfo(DWORD stream_index, IMFMediaType & input_media_type)
	: subtype_{ 0 }
	, width_(UINT32_MAX)
	, height_(UINT32_MAX)
	, framerate_numerator_(UINT32_MAX)
	, framerate_denominator_(UINT32_MAX)
	, stream_index_(stream_index)
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