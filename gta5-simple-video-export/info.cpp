#include "info.h"

#include <fstream>
#include <sstream>

extern "C" {
#include <libavutil/pixdesc.h>
}

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

auto GetAVSampleFmt(const GUID& subtype, UINT32 bits_per_sample) {
	if (subtype == MFAudioFormat_PCM) {
		switch (bits_per_sample) {
		case 8:
			return AV_SAMPLE_FMT_U8;
		case 16:
			return AV_SAMPLE_FMT_S16;
		case 32:
			return AV_SAMPLE_FMT_S32;
		case 64:
			return AV_SAMPLE_FMT_S64;
		}
	}
	else if (subtype == MFAudioFormat_Float) {
		switch (bits_per_sample) {
		case 32:
			return AV_SAMPLE_FMT_FLT;
		case 64:
			return AV_SAMPLE_FMT_DBL;
		}
	}
	return AV_SAMPLE_FMT_NONE;
}

auto GetAVPixFmt(const GUID& subtype) {
	if (subtype == MFVideoFormat_NV12) {
		return AV_PIX_FMT_NV12;
	}
	else if (subtype == MFVideoFormat_I420 || subtype == MFVideoFormat_IYUV) {
		return AV_PIX_FMT_YUV420P;
	}
	else if (subtype == MFVideoFormat_YUY2) {
		return AV_PIX_FMT_YUYV422;
	}
	else if (subtype == MFVideoFormat_RGB24) {
		return AV_PIX_FMT_RGB24;
	}
	return AV_PIX_FMT_NONE;
}

AudioInfo::AudioInfo(DWORD stream_index_, IMFMediaType & input_media_type)
	: sample_fmt{ AV_SAMPLE_FMT_NONE }
	, sample_rate{ 0 }
	, channel_layout{ 0 }
	, stream_index{ stream_index_ }
{
	LOG_ENTER;
	GUID subtype{ 0 };
	UINT32 sample_rate_u32{ 0 };
	UINT32 nb_channels{ 0 };
	UINT32 bits_per_sample{ 0 };
	LOG->debug("audio stream index = {}", stream_index);
	auto hr = input_media_type.GetGUID(MF_MT_SUBTYPE, &subtype);
	if (FAILED(hr)) {
		LOG->error("failed to get audio subtype");
	}
	else {
		LOG->info("audio subtype = {}", GUIDToString(subtype));
	}
	hr = input_media_type.GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &sample_rate_u32);
	if (FAILED(hr)) {
		LOG->error("failed to get audio sample rate");
	}
	else {
		sample_rate = sample_rate_u32;
		LOG->info("audio sample rate = {}", sample_rate);
	}
	hr = input_media_type.GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, &nb_channels);
	if (FAILED(hr)) {
		LOG->error("failed to get audio num channels");
	}
	else {
		LOG->info("audio num channels = {}", nb_channels);
		channel_layout = av_get_default_channel_layout(nb_channels);
		char buf[256];
		av_get_channel_layout_string(buf, sizeof(buf), nb_channels, channel_layout);
		LOG->info("audio channel layout = {}", buf);
		// sanity check
		if (av_get_channel_layout_nb_channels(channel_layout) != nb_channels) {
			LOG->warn("channel layout does not match num channels");
		}
	}
	hr = input_media_type.GetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, &bits_per_sample);
	if (SUCCEEDED(hr)) {
		LOG->info("audio bits per sample = {}", bits_per_sample);
	}
	sample_fmt = GetAVSampleFmt(subtype, bits_per_sample);
	if (sample_fmt != AV_SAMPLE_FMT_NONE) {
		LOG->info("audio sample format = {}", av_get_sample_fmt_name(sample_fmt));
	}
	else {
		LOG->error("failed to identify audio sample format");
	};
	LOG_EXIT;
}

VideoInfo::VideoInfo(DWORD stream_index_, IMFMediaType & input_media_type)
	: width{ 0 }
	, height{ 0 }
	, frame_rate{ 0, 1 }
	, pix_fmt{ AV_PIX_FMT_NONE }
	, stream_index{ stream_index_ }
{
	LOG_ENTER;
	GUID subtype{ 0 };
	UINT32 width_u32;
	UINT32 height_u32;
	UINT32 frame_rate_numerator;
	UINT32 frame_rate_denominator;
	LOG->debug("video stream index = {}", stream_index_);
	auto hr = input_media_type.GetGUID(MF_MT_SUBTYPE, &subtype);
	if (FAILED(hr)) {
		LOG->error("failed to get video subtype");
	}
	else {
		LOG->info("video subtype = {}", GUIDToString(subtype));
	}
	hr = MFGetAttributeSize(&input_media_type, MF_MT_FRAME_SIZE, &width_u32, &height_u32);
	if (FAILED(hr)) {
		LOG->error("failed to get video frame size");
	}
	else {
		width = width_u32;
		height = height_u32;
		LOG->info("video frame size = {}x{}", width, height);
	}
	hr = MFGetAttributeRatio(&input_media_type, MF_MT_FRAME_RATE, &frame_rate_numerator, &frame_rate_denominator);
	if (FAILED(hr)) {
		LOG->error("failed to get video frame rate");
	}
	else {
		frame_rate = AVRational{ (int)frame_rate_numerator, (int)frame_rate_denominator };
		LOG->info("video frame rate = {}/{}", frame_rate_numerator, frame_rate_denominator);
	}
	pix_fmt = GetAVPixFmt(subtype);
	if (pix_fmt != AV_PIX_FMT_NONE) {
		LOG->info("video pixel format = {}", av_get_pix_fmt_name(pix_fmt));
	}
	else {
		LOG->error("failed to identify video pixel format");
	}
	LOG_EXIT;
}