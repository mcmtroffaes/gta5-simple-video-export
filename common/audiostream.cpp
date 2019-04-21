#include "audiostream.h"

AVSampleFormat find_best_sample_fmt_of_list(const AVSampleFormat* sample_fmts, AVSampleFormat sample_fmt) {
	if (!sample_fmts) {
		return sample_fmt;
	}
	for (int i = 0; sample_fmts[i]; i++) {
		if (sample_fmts[i] == sample_fmt) {
			return sample_fmt;
		}
	}
	return sample_fmts[0];
}

AudioStream::AudioStream(AVFormatContext* format_context, AVCodecID codec_id, AVSampleFormat sample_fmt, int sample_rate, uint64_t channel_layout)
	: Stream{ format_context, codec_id }, sample_fmt{ sample_fmt }, swr{ nullptr }
{
	LOG_ENTER;
	if (context && context->codec_type != AVMEDIA_TYPE_AUDIO) {
		LOG->warn("selected audio codec does not support audio");
		avcodec_free_context(&context);
	}
	if (context) {
		if (context->codec) {
			context->sample_fmt = find_best_sample_fmt_of_list(context->codec->sample_fmts, sample_fmt);
		}
		else {
			context->sample_fmt = sample_fmt;
		}
		if (context->sample_fmt != sample_fmt) {
			LOG->info(
				"sample format {} not supported by codec so using {} instead",
				av_get_sample_fmt_name(sample_fmt), av_get_sample_fmt_name(context->sample_fmt));
		}
		context->sample_rate = sample_rate;
		context->channel_layout = channel_layout;
		context->channels = av_get_channel_layout_nb_channels(channel_layout);
		context->time_base = AVRational{ 1, sample_rate };
		int ret = 0;
		ret = avcodec_open2(context, NULL, NULL);
		if (ret < 0) {
			LOG->error("failed to open audio codec: {}", AVErrorString(ret));
		}
		if (stream) {
			avcodec_parameters_from_context(stream->codecpar, context);
			// note: this is only a hint, actual stream time_base can be different
			// avformat_write_header will set the final stream time_base
			// see https://ffmpeg.org/doxygen/trunk/structAVStream.html#a9db755451f14e2bf590d4b85d82b32e6
			stream->time_base = context->time_base;
		}
		if (frame && context->codec) {
			frame->format = context->sample_fmt;
			frame->sample_rate = context->sample_rate;
			frame->channel_layout = context->channel_layout;
			frame->channels = context->channels;
			frame->nb_samples = (context->codec->capabilities & AV_CODEC_CAP_VARIABLE_FRAME_SIZE) ? 1000 : context->frame_size;
			LOG->debug("audio buffer size is {}", frame->nb_samples);
			int ret = av_frame_get_buffer(frame, 0);
			if (ret < 0) {
				LOG->error("failed to allocate frame buffer");
			}
		}
		swr = swr_alloc_set_opts(
			NULL,
			context->channel_layout, context->sample_fmt, context->sample_rate, // out
			channel_layout,          sample_fmt,          sample_rate,          // in
			0, NULL);
		if (!swr) {
			LOG->error("failed to allocate resampling context");
		}
		ret = swr_init(swr);
		if (ret < 0) {
			LOG->error("failed to initialize resampling context");
		}
	}
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
	if (swr) {
		swr_free(&swr);
	}
	LOG_EXIT;
}
