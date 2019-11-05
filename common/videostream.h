#pragma once

#include "stream.h"

// create a video frame with empty buffer
AVFramePtr CreateVideoFrame(int width, int height, AVPixelFormat pix_fmt);

// create a video frame whose buffer is managed externally by ptr
AVFramePtr CreateVideoFrame(int width, int height, AVPixelFormat pix_fmt, uint8_t* ptr);

class VideoStream :
	public Stream
{
private:
	// native pixel format as passed to the constructor (this can be different from context->pix_fmt)
	const AVPixelFormat pix_fmt;

	// frame for encoder (converted from the src_frame)
	AVFramePtr dst_frame;

public:
	// set up stream with the given parameters
	VideoStream(std::shared_ptr<AVFormatContext>& format_context, AVCodecID codec_id, int width, int height, const AVRational& frame_rate, AVPixelFormat pix_fmt);

	// encode the frame to a format that is compatible with the codec
	// (needs to match width, height, and pix_fmt, as specified in constructor)
	void Transcode(const AVFramePtr& src_frame);
};
