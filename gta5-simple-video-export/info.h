/*
Classes to store information about the export.

AudioInfo stores information about the audio format (bit rate, number of
channels, and so on). It also stores the handle to the raw audio file, and the
media foundation stream index. This information is initialized in the
SinkWriterSetInputMediaType hook  (search for "audio_info.reset").

VideoInfo stores information about the video format (resolution, framerate,
and so on). It also stores the handle to the raw video file, and the
media foundation stream index. This information is initialized in the
SinkWriterSetInputMediaType hook  (search for "video_info.reset").

Uninitialized values are represented by UINT32_MAX for integer
variables.

Each info class also has an UpdateSettings method, which exports the
information to the DEFAULT section of the ini file. This happens in the
SinkWriterBeginWriting hook, just before the ini file is interpolated (search
for "settings->interpolate").

CreateBatchFile is a function that creates a batch file for converting
the raw files to a compressed audio/video container file, using the
batch_command specified by the user in the ini file.
*/

#pragma once

#include "logger.h"
#include "settings.h"
#include "filehandle.h"

#include <mfapi.h>
#pragma comment(lib, "mfuuid.lib")

class AudioInfo {
private:
	GUID subtype_;
	uint32_t rate_;
	uint32_t num_channels_;
	uint32_t bits_per_sample_;
public:
	DWORD stream_index_;
	std::unique_ptr<FileHandle> handle_;
	AudioInfo(DWORD stream_index, IMFMediaType & input_media_type);
	void UpdateSettings(Settings & settings) const;
};

class VideoInfo {
private:
	GUID subtype_;
	uint32_t width_;
	uint32_t height_;
	uint32_t framerate_numerator_;
	uint32_t framerate_denominator_;
public:
	DWORD stream_index_;
	std::unique_ptr<FileHandle> handle_;
	VideoInfo(DWORD stream_index, IMFMediaType & input_media_type);
	void UpdateSettings(Settings & settings) const;
};

void CreateBatchFile(const Settings & settings);