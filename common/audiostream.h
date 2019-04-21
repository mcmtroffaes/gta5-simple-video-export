#pragma once

#include "stream.h"


class AudioStream :
	public Stream
{
public:
	const AVSampleFormat sample_fmt; // native sample format as passed to the constructor (this can be different from context->sample_fmt)

	AudioStream(AVFormatContext* format_context, AVCodecID codec_id, AVSampleFormat sample_fmt, int sample_rate, uint64_t channel_layout);

	void Encode();
};

