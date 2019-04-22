#pragma once

#include "stream.h"

class VideoStream :
	public Stream
{
private:
	// native pixel format as passed to the constructor (this can be different from context->pix_fmt)
	const AVPixelFormat pix_fmt;

public:
	// set up stream with the given parameters
	VideoStream(AVFormatContext* format_context, AVCodecID codec_id, int width, int height, const AVRational& frame_rate, AVPixelFormat pix_fmt);

	// transcode the data to a format that is compatible with the codec
	// (needs to match width, height, and pix_fmt, as specified in constructor)
	void Transcode(uint8_t* ptr);
};
