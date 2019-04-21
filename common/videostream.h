#pragma once

#include "stream.h"

class VideoStream :
	public Stream
{
public:
	const AVPixelFormat pix_fmt; // native pixel format as passed to the constructor (this can be different from context->pix_fmt)

	VideoStream(AVFormatContext* format_context, AVCodecID codec_id, int width, int height, const AVRational& frame_rate, AVPixelFormat pix_fmt);

	void MakeFrame(uint8_t* ptr);
};
