#pragma once

#include "logger.h"
#include "settings.h"
#include "filehandle.h"

#include <mfapi.h>
#pragma comment(lib, "mfuuid.lib")

// information about the export, available through variables in the ini file
// for integer variables, UINT32_MAX represents an uninitialized value
// for strings, an empty string represents an uninitialized value

class GeneralInfo {
private:
	std::string documentsfolder_;
	std::string videosfolder_;
	std::string exportfolder_;
	std::string timestamp_;
public:
	GeneralInfo(const Settings & settings);
	void Substitute(std::string & str) const;
};

class AudioInfo {
private:
	std::string audio_format_;
	std::string audio_path_;
	uint32_t audio_rate_;
	uint32_t audio_num_channels_;
	uint32_t audio_bits_per_sample_;
public:
	DWORD stream_index_;
	std::unique_ptr<FileHandle> os_;
	AudioInfo(DWORD stream_index, IMFMediaType & input_media_type, const Settings & settings, const GeneralInfo & info);
	void Substitute(std::string & str) const;
};

class VideoInfo {
private:
	std::string video_format_;
	std::string video_path_;
	uint32_t width_;
	uint32_t height_;
	uint32_t framerate_numerator_;
	uint32_t framerate_denominator_;
public:
	DWORD stream_index_;
	std::unique_ptr<FileHandle> os_;
	VideoInfo(DWORD stream_index, IMFMediaType & input_media_type, const Settings & settings, const GeneralInfo & info);
	void Substitute(std::string & str) const;
};

void CreateClientBatchFile(const Settings & settings, const GeneralInfo & info, const AudioInfo & audio_info, const VideoInfo & video_info);