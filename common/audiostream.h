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
	AudioStream(std::shared_ptr<AVFormatContext>& format_context, AVCodecID codec_id, AVSampleFormat sample_fmt, int sample_rate, uint64_t channel_layout);

	// transcode the data to a format that is compatible with the codec
	// (needs to match sample_fmt and channel_layout as specified in constructor)
	void Transcode(uint8_t *ptr, int nb_samples);

	~AudioStream();
};
