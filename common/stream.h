#pragma once

#include "logger.h"

extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/timestamp.h>
}

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
	void SendFrame();

	// current frame time (in seconds)
	double Time() const;

	// unallocate frame
	// flush encoder, unallocate codec context
	// stream itself is unallocated by the format context
	~Stream();
};
