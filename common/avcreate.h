#pragma once

#include <memory>
#include <string>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/audio_fifo.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}

struct AVFormatContextDeleter { void operator()(AVFormatContext* context) const; };
struct AVStreamDeleter { void operator()(AVStream* stream) const; };
struct AVCodecContextDeleter { void operator()(AVCodecContext* context) const; };
struct AVFrameDeleter { void operator()(AVFrame* frame) const; };
struct SwrContextDeleter { void operator()(SwrContext* context) const; };
struct SwsContextDeleter { void operator()(SwsContext* context) const; };
struct AVAudioFifoDeleter { void operator()(AVAudioFifo* fifo) const; };

using AVFormatContextPtr = std::unique_ptr<AVFormatContext, AVFormatContextDeleter>;
using AVCodecPtr = const AVCodec*;
using AVStreamPtr = std::unique_ptr<AVStream, AVStreamDeleter>;
using AVCodecContextPtr = std::unique_ptr<AVCodecContext, AVCodecContextDeleter>;
using AVFramePtr = std::unique_ptr<AVFrame, AVFrameDeleter>;
using SwrContextPtr = std::unique_ptr<SwrContext, SwrContextDeleter>;
using SwsContextPtr = std::unique_ptr<SwsContext, SwsContextDeleter>;
using AVAudioFifoPtr = std::unique_ptr<AVAudioFifo, AVAudioFifoDeleter>;

AVFormatContextPtr CreateAVFormatContext(const std::string& filename);
AVCodecPtr CreateAVCodec(const AVCodecID& codec_id);
AVStreamPtr CreateAVStream(AVFormatContext& format_context, const AVCodec& codec);
AVCodecContextPtr CreateAVCodecContext(const AVCodec& codec);
AVFramePtr CreateAVFrame();
SwrContextPtr CreateSwrContext(
	uint64_t out_channel_layout, AVSampleFormat out_sample_fmt, int out_sample_rate,
	uint64_t in_channel_layout, AVSampleFormat in_sample_fmt, int in_sample_rate);
SwsContextPtr CreateSwsContext(
	int srcW, int srcH, AVPixelFormat srcFormat,
	int dstW, int dstH, AVPixelFormat dstFormat,
	int flags);
AVAudioFifoPtr CreateAVAudioFifo(AVSampleFormat sample_fmt, int channels, int nb_samples);
