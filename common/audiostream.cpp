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

AudioStream::AudioStream(std::shared_ptr<AVFormatContext>& format_context, AVCodecID codec_id, AVSampleFormat sample_fmt, int sample_rate, uint64_t channel_layout)
	: Stream{ format_context, codec_id }
	, sample_fmt{ sample_fmt }, sample_rate{ sample_rate }
	, channel_layout{ channel_layout }, channels { av_get_channel_layout_nb_channels(channel_layout) }
	, swr{ nullptr }, fifo{ nullptr }
{
	LOG_ENTER;
	if (context->codec_type != AVMEDIA_TYPE_AUDIO)
		LOG_THROW(std::invalid_argument, "selected audio codec does not support audio");
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
	ret = avcodec_open2(context.get(), NULL, NULL);
	if (ret < 0)
		LOG_THROW(std::runtime_error, fmt::format("failed to open audio codec: {}", AVErrorString(ret)));
	avcodec_parameters_from_context(stream->codecpar, context.get());
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
	ret = av_frame_get_buffer(frame.get(), 0);
	if (ret < 0)
		LOG_THROW(std::runtime_error, fmt::format("failed to allocate frame buffer: {}", AVErrorString(ret)));
	swr = swr_alloc_set_opts(
		NULL,
		context->channel_layout, context->sample_fmt, context->sample_rate, // out
		channel_layout,          sample_fmt,          sample_rate,          // in
		0, NULL);
	if (!swr)
		LOG_THROW(std::runtime_error, "failed to allocate resampling context");
	ret = swr_init(swr);
	if (ret < 0)
		LOG_THROW(std::runtime_error, fmt::format("failed to initialize resampling context: {}", AVErrorString(ret)));
	fifo = av_audio_fifo_alloc(context->sample_fmt, context->channels, frame->nb_samples);
	if (!fifo)
		LOG_THROW(std::runtime_error, "failed to allocate audio buffer");
	LOG_EXIT;
}

void AudioStream::Transcode(uint8_t* ptr, int nb_samples)
{
	LOG_ENTER;
	uint8_t* src_data[4]{ nullptr, nullptr, nullptr, nullptr };
	int src_linesize[4]{ 0, 0, 0, 0 };
	// convert source pointer to standard data/linesize format
	// this fails if ptr is null (which happens for a flush), so check for that case
	if (ptr) {
		int ret = av_samples_fill_arrays(src_data, src_linesize, ptr, channels, nb_samples, sample_fmt, 1);
		if (ret < 0)
			LOG_THROW(std::runtime_error, fmt::format("failed to fill audio sample arrays: {}", AVErrorString(ret)));
	}
	// number of samples required in the destination buffer (to avoid buffering)
	int dst_nb_samples = swr_get_out_samples(swr, nb_samples);
	if (dst_nb_samples < 0)
		LOG_THROW(std::runtime_error, fmt::format("failed to get number of samples from audio resampler: {}", AVErrorString(dst_nb_samples)));
	// av_samples_alloc can fail if dst_nb_samples is zero
	// so let's allocate at least one sample, even if we're flushing the audio buffer
	if (dst_nb_samples == 0)
		dst_nb_samples = 1;
	// allocate destination buffer
	uint8_t* dst_data[4];
	int dst_linesize[4];
	int ret = av_samples_alloc(dst_data, dst_linesize, context->channels, dst_nb_samples, context->sample_fmt, 0);
	if (ret < 0)
		LOG_THROW(std::runtime_error, fmt::format("failed to allocate audio destination buffer: {}", AVErrorString(ret)));
	// resample source data to destination buffer
	int nb_convert = swr_convert(swr, dst_data, dst_nb_samples, (const uint8_t **)src_data, nb_samples);
	if (nb_convert < 0) {
		av_freep(&dst_data[0]);
		LOG_THROW(std::runtime_error, fmt::format("resampling error: {}", AVErrorString(nb_convert)));
	}
	int nb_written = av_audio_fifo_write(fifo, (void**) dst_data, nb_convert);
	if (nb_written < 0)
		LOG_THROW(std::runtime_error, fmt::format("failed to write to audio buffer: {}", AVErrorString(nb_written)));
	if (nb_written != nb_convert)
		LOG->warn("expected {} samples to be written to audio buffer but wrote {}", nb_convert, nb_written);
	// free destination buffer
	av_freep(&dst_data[0]);
	// read from fifo buffer in chunks of frame->nb_samples, until no further chunks can be read
	while (av_audio_fifo_size(fifo) >= frame->nb_samples) {
		int nb_read = av_audio_fifo_read(fifo, (void**)frame->data, frame->nb_samples);
		if (nb_read < 0)
			LOG_THROW(std::runtime_error, fmt::format("audio buffer read error: {}", AVErrorString(nb_read)));
		if (nb_read != frame->nb_samples)
			LOG->warn("expected {} samples from audio buffer but got {}", frame->nb_samples, nb_read);
		Encode(frame);
		frame->pts += nb_read;
	}
	// flush buffer if needed
	if (!ptr) {
		int nb_read = av_audio_fifo_read(fifo, (void**)frame->data, frame->nb_samples);
		if (nb_read < 0)
			LOG_THROW(std::runtime_error, fmt::format("audio buffer read error: {}", AVErrorString(nb_read)));
		frame->nb_samples = nb_read;
		Encode(frame);
		frame->pts += nb_read;
		Encode(nullptr);
		int nb_lost = av_audio_fifo_size(fifo);
		if (nb_lost)
			LOG->warn("audio buffer not completely flushed, {} samples lost");
	}
	LOG_EXIT;
}

AudioStream::~AudioStream()
{
	LOG_ENTER;
	av_audio_fifo_free(fifo);
	swr_free(&swr);
	LOG_EXIT;
}
