/*
Classes to store information about the export.

AudioInfo stores information about the audio format (bit rate, number of
channels, and so on). It also stores the
media foundation stream index. This information is initialized in the
SinkWriterSetInputMediaType hook (search for "std::make_unique<AudioInfo>").

VideoInfo stores information about the video format (resolution, framerate,
and so on). It also stores the handle to the
media foundation stream index. This information is initialized in the
SinkWriterSetInputMediaType hook (search for "std::make_unique<VideoInfo>").
*/

#pragma once

#include "logger.h"
#include "settings.h"

#include <windows.h>
#include <mfapi.h>
#pragma comment(lib, "mfuuid.lib")

extern "C" {
#include <libavcodec/avcodec.h>
}

class AudioInfo {
public:
	AVSampleFormat sample_fmt;
	int sample_rate;
	uint64_t channel_layout;
	DWORD stream_index;
	AudioInfo(DWORD stream_index, IMFMediaType& input_media_type);
};

class VideoInfo {
public:
	int width;
	int height;
	AVRational frame_rate;
	AVPixelFormat pix_fmt;
	DWORD stream_index;
	VideoInfo(DWORD stream_index, IMFMediaType& input_media_type);
};
