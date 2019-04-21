#pragma once

#include "logger.h"

extern "C" {
#include <libavformat/avformat.h>
}

// A class for encoding frames to an AVStream.
// Usage:
// * Create a format context with avformat_alloc_output_context2.
// * Create Stream objects (passing the created format context).
// * Write the format header with avformat_write_header.
// * For each frame you want to encode:
//     - Copy your data to Stream::frame.
//     - Call Stream::Encode() to encode this frame.
// * Destroy the Stream objects (it is important to do this before writing the trailer, because this will flush the encoder).
// * Write the format trailer with av_write_trailer.
// * Destroy the format context.
// It is important not to destroy the format context as long as the Stream object is in use.
class Stream {
public:
	AVFormatContext* format_context; // context which owns this stream
	AVStream* stream;                // the stream
	AVCodecContext* context;         // codec context for this stream
	AVFrame* frame;                  // pre-allocated frame for storage during encoding

	// add stream to the given format context, and initialize codec context and frame
	// note: frame buffer is not allocated (we do not know the stream format yet at this point)
	// note: frame->pts is set to zero
	Stream(AVFormatContext* format_context, AVCodecID codec_id);

	// send frame to the encoder
	void Encode();

	// current frame time (in seconds)
	double Time() const;

	// unallocate frame
	// flush encoder, unallocate codec context
	// stream itself is unallocated by the format context
	~Stream();
};
