/*
Classes to store information about the export.

The information stored here is available to the user through variables in the
ini file. Each class exposes a Substitute method to perform this variable
substitution. Whenever some variable from the ini file is used, these
Substitute methods are to be called.

In the design, uninitialized values are represented by UINT32_MAX for integer
variables, and by an empty string for string variables.

Info stores general information available from the start of the export, such
as timestamp, and various folder locations. This information is initialized in
the CreateSinkWriterFromURL hook (search for "info.reset").

AudioInfo stores information about the audio format (bit rate, number of
channels, and so on). It also stores the handle to the raw audio file, and the
media foundation stream index. This information is initialized in the
SinkWriterSetInputMediaType hook  (search for "audio_info.reset").

VideoInfo stores information about the video format (resolution, framerate,
and so on). It also stores the handle to the raw video file, and the
media foundation stream index. This information is initialized in the
SinkWriterSetInputMediaType hook  (search for "video_info.reset").

CreateClientBatchFile is a function that creates a batch file for converting
the raw files to a compressed audio/video container file, using the command
and arguments specified by the user in the ini file.
*/

#pragma once

#include "logger.h"
#include "settings.h"
#include "filehandle.h"

#include <mfapi.h>
#pragma comment(lib, "mfuuid.lib")

class Info {
private:
	std::wstring documentsfolder_;
	std::wstring videosfolder_;
	std::wstring exportfolder_;
	std::wstring timestamp_;
public:
	Info(const Settings & settings);
	void Substitute(std::wstring & str) const;
};

class AudioInfo {
private:
	std::wstring audio_format_;
	std::wstring audio_path_;
	uint32_t audio_rate_;
	uint32_t audio_num_channels_;
	uint32_t audio_bits_per_sample_;
public:
	DWORD stream_index_;
	std::unique_ptr<FileHandle> handle_;
	AudioInfo(DWORD stream_index, IMFMediaType & input_media_type, const Settings & settings, const Info & info);
	void Substitute(std::wstring & str) const;
};

class VideoInfo {
private:
	std::wstring video_format_;
	std::wstring video_path_;
	uint32_t width_;
	uint32_t height_;
	uint32_t framerate_numerator_;
	uint32_t framerate_denominator_;
public:
	DWORD stream_index_;
	std::unique_ptr<FileHandle> handle_;
	VideoInfo(DWORD stream_index, IMFMediaType & input_media_type, const Settings & settings, const Info & info);
	void Substitute(std::wstring & str) const;
};

void CreateClientBatchFile(const Settings & settings, const Info & info, const AudioInfo & audio_info, const VideoInfo & video_info);