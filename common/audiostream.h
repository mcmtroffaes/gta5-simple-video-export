#pragma once

#include "stream.h"

extern "C" {
#include <libswresample/swresample.h>
#include <libavutil/audio_fifo.h>
}

struct SwrContextDeleter { void operator()(SwrContext* context) const; };
struct AVAudioFifoDeleter { void operator()(AVAudioFifo* fifo) const; };

using SwrContextPtr = std::unique_ptr<SwrContext, SwrContextDeleter>;
using AVAudioFifoPtr = std::unique_ptr<AVAudioFifo, AVAudioFifoDeleter>;

SwrContextPtr CreateSwrContext(
	uint64_t out_channel_layout, AVSampleFormat out_sample_fmt, int out_sample_rate,
	uint64_t in_channel_layout, AVSampleFormat in_sample_fmt, int in_sample_rate);
AVAudioFifoPtr CreateAVAudioFifo(AVSampleFormat sample_fmt, int channels, int nb_samples);

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
	SwrContextPtr swr;

	// audio fifo buffer
	AVAudioFifoPtr fifo;

public:
	// set up stream with the given parameters
	AudioStream(std::shared_ptr<AVFormatContext>& format_context, AVCodecID codec_id, AVSampleFormat sample_fmt, int sample_rate, uint64_t channel_layout);

	// transcode the data to a format that is compatible with the codec
	// (needs to match sample_fmt and channel_layout as specified in constructor)
	void Transcode(uint8_t *ptr, int nb_samples);
};
