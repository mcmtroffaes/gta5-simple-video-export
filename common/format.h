#pragma once

#include "logger.h"
#include "videostream.h"
#include "audiostream.h"

class Format
{
private:
	std::shared_ptr<AVFormatContext> context;

public:
	VideoStream vstream;
	AudioStream astream;

	Format(
		const std::filesystem::path& filename,
		AVCodecID vcodec, AVDictionaryPtr& voptions, int width, int height, const AVRational& frame_rate, AVPixelFormat pix_fmt,
		AVCodecID acodec, AVDictionaryPtr& aoptions, AVSampleFormat sample_fmt, int sample_rate, uint64_t channel_layout);

	// flush streams and write the footer
	void Flush();
};

