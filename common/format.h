#pragma once

#include "logger.h"
#include "videostream.h"
#include "audiostream.h"

class Format
{
private:
	AVFormatContext* context;

public:
	std::unique_ptr<VideoStream> vstream;
	std::unique_ptr<AudioStream> astream;

	Format(
		const std::string& filename,
		AVCodecID vcodec, int width, int height, const AVRational& frame_rate, AVPixelFormat pix_fmt,
		AVCodecID acodec, AVSampleFormat sample_fmt, int sample_rate, uint64_t channel_layout);

	void Flush();

	~Format();
};

