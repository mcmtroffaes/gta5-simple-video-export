#pragma once

#include "logger.h"

extern "C" {
#include <libavformat/avformat.h>
}

struct AVStreamDeleter { void operator()(AVStream* stream); };

using AVFormatContextPtr = std::shared_ptr<AVFormatContext>;
using AVCodecPtr = const AVCodec*;
using AVStreamPtr = std::unique_ptr<AVStream, AVStreamDeleter>;

AVStreamPtr CreateAVStream(const AVFormatContextPtr& format_context, const AVCodecPtr& codec);

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
	template<typename T>
	using deleted_unique_ptr = std::unique_ptr<T, std::function<void(T*)>>;

	std::weak_ptr<AVFormatContext> owner;       // context which owns this stream
	AVCodecPtr codec;                           // the codec
	AVStreamPtr stream;                         // the stream
	deleted_unique_ptr<AVCodecContext> context; // codec context for this stream
	deleted_unique_ptr<AVFrame> frame;          // a pre-allocated frame that can be used for encoding

	// add stream to the given format context, and initialize codec context and frame
	// note: frame buffer is not allocated (we do not know the stream format yet at this point)
	// note: frame->pts is set to zero
	Stream(std::shared_ptr<AVFormatContext>& format_context, AVCodecID codec_id);

	// send frame to the encoder
	void Encode(const deleted_unique_ptr<AVFrame>& avframe);

	// current frame time (in seconds)
	double Time() const;
};
