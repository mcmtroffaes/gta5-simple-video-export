#pragma once

#include "logger.h"
#include "avcreate.h"

extern "C" {
#include <libavformat/avformat.h>
}

// A class for encoding frames to an AVStream.
// Usage:
// * Create a format context with avformat_alloc_output_context2.
// * Create Stream objects (passing the created format context).
// * Write the format header with avformat_write_header.
// * For each frame you want to encode:
//     - Set up an AVFrame.
//     - Call Stream::Encode(frame).
// * Call Stream::Encode(nullptr) to flush the encoder.
// * Write the format trailer with av_write_trailer.
// * Destroy the format context.
// It is important not to destroy the format context as long as the Stream object is in use.
class Stream {
public:
	std::weak_ptr<AVFormatContext> owner; // context which owns this stream
	AVStreamPtr stream;           // the stream
	AVCodecContextPtr context;    // codec context for this stream

	// add stream to the given format context, and initialize codec context and frame
	// note: frame buffer is not allocated (we do not know the stream format yet at this point)
	// note: frame->pts is set to zero
	Stream(std::shared_ptr<AVFormatContext>& format_context, const AVCodec& codec);

	// send frame to the encoder
	void Encode(const AVFramePtr& avframe);
};
