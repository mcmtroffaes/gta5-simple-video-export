#pragma once

#include "stream.h"
#include "avcreate.h"

extern "C" {
#include <libswresample/swresample.h>
#include <libavutil/audio_fifo.h>
}

// create an audio frame but do not allocate a buffer
AVFramePtr CreateAudioFrame(AVSampleFormat sample_fmt, int sample_rate, uint64_t channel_layout);

// create an audio frame whose buffer is managed internally
AVFramePtr CreateAudioFrame(AVSampleFormat sample_fmt, int sample_rate, uint64_t channel_layout, int nb_samples);

// create an audio frame whose buffer is managed externally by ptr
AVFramePtr CreateAudioFrame(AVSampleFormat sample_fmt, int sample_rate, uint64_t channel_layout, int nb_samples, const uint8_t* ptr);

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

	// frame for encoder (converted from the src_frame)
	AVFramePtr dst_frame;

public:
	// set up stream with the given parameters
	AudioStream(std::shared_ptr<AVFormatContext>& format_context, const AVCodec& codec, AVDictionaryPtr& options, AVSampleFormat sample_fmt, int sample_rate, uint64_t channel_layout);

	// transcode the data to a format that is compatible with the codec
	// (needs to match sample_fmt and channel_layout as specified in constructor)
	void Transcode(const AVFramePtr& src_frame);
};
