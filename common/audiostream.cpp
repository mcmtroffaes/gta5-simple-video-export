#include "audiostream.h"

auto GetChannelLayoutString(uint64_t channel_layout) {
	char buf[64]{ 0 };
	av_get_channel_layout_string(buf, sizeof(buf), 0, channel_layout);
	return std::string(buf);
}

auto FindBestSampleFmt(const AVCodec* codec, AVSampleFormat sample_fmt) {
	LOG_ENTER;
	auto best_sample_fmt{ sample_fmt };
	auto supported_sample_fmt{ codec ? codec->sample_fmts : NULL };
	if (supported_sample_fmt) {
		best_sample_fmt = *supported_sample_fmt;
		while (*(++supported_sample_fmt) != AV_SAMPLE_FMT_NONE) {
			if (*supported_sample_fmt == sample_fmt) {
				best_sample_fmt = sample_fmt;
				break;
			}
		}
	}
	LOG_EXIT;
	return best_sample_fmt;
}

auto FindBestSampleRate(const AVCodec* codec, int sample_rate) {
	LOG_ENTER;
	auto best_sample_rate{ sample_rate };
	auto supported_sample_rate{ codec ? codec->supported_samplerates : NULL };
	if (supported_sample_rate) {
		best_sample_rate = *supported_sample_rate;
		while (*(++supported_sample_rate))
			if (abs(sample_rate - *supported_sample_rate) < abs(sample_rate - best_sample_rate))
				best_sample_rate = *supported_sample_rate;
	}
	LOG_EXIT;
	return best_sample_rate;
}

auto FindBestChannelLayout(const AVCodec* codec, uint64_t channel_layout) {
	LOG_ENTER;
	auto best_layout{ channel_layout };
	auto supported_layout{ codec ? codec->channel_layouts : NULL };
	if (supported_layout) {
		best_layout = *supported_layout;
		while (*(++supported_layout)) {
			if (*supported_layout == channel_layout) {
				best_layout = channel_layout;
				break;
			}
		}
	}
	LOG_EXIT;
	return best_layout;
}

AudioStream::AudioStream(AVFormatContext* format_context, AVCodecID codec_id, AVSampleFormat sample_fmt, int sample_rate, uint64_t channel_layout)
	: Stream{ format_context, codec_id }
	, sample_fmt{ sample_fmt }, sample_rate{ sample_rate }
	, channel_layout{ channel_layout }, channels { av_get_channel_layout_nb_channels(channel_layout) }
	, swr{ nullptr }, fifo{ nullptr }
{
	LOG_ENTER;
	if (context->codec_type != AVMEDIA_TYPE_AUDIO)
		throw std::invalid_argument("selected audio codec does not support audio");
	context->sample_fmt = FindBestSampleFmt(context->codec, sample_fmt);
	if (context->sample_fmt != sample_fmt)
		LOG->info(
			"codec {} does not support sample format {} so using {}",
			avcodec_get_name(codec_id),
			av_get_sample_fmt_name(sample_fmt),
			av_get_sample_fmt_name(context->sample_fmt));
	context->sample_rate = FindBestSampleRate(context->codec, sample_rate);
	if (context->sample_rate != sample_rate)
		LOG->info(
			"codec {} does not support sample rate {} so using {}",
			avcodec_get_name(codec_id), sample_rate, context->sample_rate);
	context->channel_layout = FindBestChannelLayout(context->codec, channel_layout);
	if (context->channel_layout != channel_layout)
		LOG->info(
			"codec {} does not support channel layout {} so using {}",
			avcodec_get_name(codec_id),
			GetChannelLayoutString(channel_layout),
			GetChannelLayoutString(context->channel_layout));
	context->channels = av_get_channel_layout_nb_channels(context->channel_layout);
	context->time_base = AVRational{ 1, context->sample_rate };
	int ret = 0;
	ret = avcodec_open2(context, NULL, NULL);
	if (ret < 0)
		throw std::runtime_error(fmt::format("failed to open audio codec: {}", AVErrorString(ret)));
	avcodec_parameters_from_context(stream->codecpar, context);
	// note: this is only a hint, actual stream time_base can be different
	// avformat_write_header will set the final stream time_base
	// see https://ffmpeg.org/doxygen/trunk/structAVStream.html#a9db755451f14e2bf590d4b85d82b32e6
	stream->time_base = context->time_base;
	frame->format = context->sample_fmt;
	frame->sample_rate = context->sample_rate;
	frame->channel_layout = context->channel_layout;
	frame->channels = context->channels;
	frame->nb_samples = (context->codec->capabilities & AV_CODEC_CAP_VARIABLE_FRAME_SIZE) ? 1000 : context->frame_size;
	LOG->debug("codec frame size is {}", frame->nb_samples);
	ret = av_frame_get_buffer(frame, 0);
	if (ret < 0)
		throw std::runtime_error(fmt::format("failed to allocate frame buffer: {}", AVErrorString(ret)));
	swr = swr_alloc_set_opts(
		NULL,
		context->channel_layout, context->sample_fmt, context->sample_rate, // out
		channel_layout,          sample_fmt,          sample_rate,          // in
		0, NULL);
	if (!swr)
		throw std::runtime_error("failed to allocate resampling context");
	ret = swr_init(swr);
	if (ret < 0)
		throw std::runtime_error(fmt::format("failed to initialize resampling context: {}", AVErrorString(ret)));
	fifo = av_audio_fifo_alloc(context->sample_fmt, context->channels, context->frame_size);
	if (!fifo)
		throw std::runtime_error("failed to allocate audio buffer");
	LOG_EXIT;
}

void AudioStream::Encode(uint8_t* ptr)
{
	LOG_ENTER;
	uint8_t* data[4];
	int linesize[4];
	av_samples_fill_arrays(data, linesize, ptr, context->channels, frame->nb_samples, sample_fmt, 1);
	int ret = swr_convert(swr, frame->data, frame->nb_samples, (const uint8_t **)data, frame->nb_samples);
	if (ret < 0) {
		LOG->error("resampling error: {}", AVErrorString(ret));
	}
	else if (frame->nb_samples != ret) {
		LOG->error("expected {} samples but got {}", frame->nb_samples, ret);
	};
	Stream::Encode();
	frame->pts += frame->nb_samples;
	LOG_EXIT;
}

AudioStream::~AudioStream()
{
	LOG_ENTER;
	if (fifo) {
		av_audio_fifo_free(fifo);
	}
	if (swr) {
		swr_free(&swr);
	}
	LOG_EXIT;
}
