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
	// parameters as passed to constructor (which can be different from encoder context)
	const AVSampleFormat sample_fmt;
	const int sample_rate;
	const uint64_t channel_layout;
	const int channels;

	// resampler context
	SwrContext* swr;

	// audio fifo buffer
	AVAudioFifo* fifo;

public:
	// set up stream with the given parameters
	AudioStream(AVFormatContext* format_context, AVCodecID codec_id, AVSampleFormat sample_fmt, int sample_rate, uint64_t channel_layout);

	// encode the data (needs to match sample_fmt and channel_layout as specified in constructor)
	void Encode(uint8_t *ptr, int nb_samples);

	~AudioStream();
};
