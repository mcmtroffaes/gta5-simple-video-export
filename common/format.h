#pragma once

#include "logger.h"
#include "videostream.h"
#include "audiostream.h"

class Format
{
private:
	std::shared_ptr<AVFormatContext> context;

public:
	std::unique_ptr<VideoStream> vstream;
	std::unique_ptr<AudioStream> astream;

	Format(
		const std::filesystem::path& filename,
		AVCodecID vcodec, int width, int height, const AVRational& frame_rate, AVPixelFormat pix_fmt,
		AVCodecID acodec, AVSampleFormat sample_fmt, int sample_rate, uint64_t channel_layout);

	// flush streams and write the footer
	void Flush();
};

