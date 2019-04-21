#pragma once

#include "logger.h"

extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/timestamp.h>
}

class Stream {
public:
	AVStream* stream;
	AVCodecContext* context;
	AVFrame* frame;

	Stream(AVFormatContext* format_context, AVCodecID codec_id);

	void SendFrame(AVFormatContext* format_context);

	void Flush(AVFormatContext* format_context);

	double Time() const;

	~Stream();
};
