#pragma once

#include "stream.h"

extern "C" {
#include <libswresample/swresample.h>
#include <libavutil/audio_fifo.h>
}

class AudioStream :
	public Stream
{
private:
	// native sample format as passed to the constructor (this can be different from context->sample_fmt)
	const AVSampleFormat sample_fmt;

	// resampler context
	SwrContext* swr;

	// audio fifo buffer
	AVAudioFifo* fifo;

public:
	// set up stream with the given parameters
	AudioStream(AVFormatContext* format_context, AVCodecID codec_id, AVSampleFormat sample_fmt, int sample_rate, uint64_t channel_layout);

	// encode the data (needs to match sample_fmt and channel_layout as specified in constructor)
	// number of samples needs to match exactly frame->nb_samples
	// TODO allow arbitrary number of samples to be passed here, use AVAudioFifo interface
	void Encode(uint8_t *ptr);

	~AudioStream();
};
