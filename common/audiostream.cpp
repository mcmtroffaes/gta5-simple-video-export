#include "audiostream.h"

AVFramePtr CreateAudioFrame(AVSampleFormat sample_fmt, int sample_rate, uint64_t channel_layout) {
	LOG_ENTER;
	auto frame = CreateAVFrame();
	frame->format = sample_fmt;
	frame->sample_rate = sample_rate;
	frame->channel_layout = channel_layout;
	frame->channels = av_get_channel_layout_nb_channels(channel_layout);
	LOG_EXIT;
	return frame;
}

AVFramePtr CreateAudioFrame(AVSampleFormat sample_fmt, int sample_rate, uint64_t channel_layout, int nb_samples) {
	LOG_ENTER;
	auto frame = CreateAudioFrame(sample_fmt, sample_rate, channel_layout);
	frame->nb_samples = nb_samples;
	int ret = av_frame_get_buffer(frame.get(), 0);
	if (ret < 0)
		LOG_THROW(std::runtime_error, fmt::format("failed to allocate frame buffer: {}", AVErrorString(ret)));
	LOG_EXIT;
	return frame;
}

AVFramePtr CreateAudioFrame(AVSampleFormat sample_fmt, int sample_rate, uint64_t channel_layout, int nb_samples, const uint8_t* ptr) {
	LOG_ENTER;
	auto frame = CreateAudioFrame(sample_fmt, sample_rate, channel_layout);
	frame->nb_samples = nb_samples;
	int ret = av_samples_fill_arrays(frame->data, frame->linesize, ptr, frame->channels, nb_samples, sample_fmt, 1);
	if (ret < 0)
		LOG_THROW(std::runtime_error, fmt::format("failed to fill audio sample arrays: {}", AVErrorString(ret)));
	LOG_EXIT;
	return frame;
}

auto GetChannelLayoutString(uint64_t channel_layout) {
	LOG_ENTER;
	char buf[64]{ 0 };
	av_get_channel_layout_string(buf, sizeof(buf), 0, channel_layout);
	LOG_EXIT;
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

AudioStream::AudioStream(std::shared_ptr<AVFormatContext>& format_context, AVCodecID codec_id, AVDictionaryPtr& options, AVSampleFormat sample_fmt, int sample_rate, uint64_t channel_layout)
	: Stream{ format_context, codec_id }
	, sample_fmt{ sample_fmt }, sample_rate{ sample_rate }
	, channel_layout{ channel_layout }, channels { av_get_channel_layout_nb_channels(channel_layout) }
	, dst_frame{ nullptr }, swr{ nullptr }, fifo{ nullptr }
{
	LOG_ENTER_METHOD;
	if (context->codec_type != AVMEDIA_TYPE_AUDIO)
		LOG_THROW(std::invalid_argument, "selected audio codec does not support audio");
	context->sample_fmt = FindBestSampleFmt(context->codec, sample_fmt);
	if (context->sample_fmt != sample_fmt)
		LOG->info(
			"codec {} does not support sample format {} so transcoding to {}",
			avcodec_get_name(codec_id),
			av_get_sample_fmt_name(sample_fmt),
			av_get_sample_fmt_name(context->sample_fmt));
	context->sample_rate = FindBestSampleRate(context->codec, sample_rate);
	if (context->sample_rate != sample_rate)
		LOG->info(
			"codec {} does not support sample rate {} so transcoding to {}",
			avcodec_get_name(codec_id), sample_rate, context->sample_rate);
	context->channel_layout = FindBestChannelLayout(context->codec, channel_layout);
	if (context->channel_layout != channel_layout)
		LOG->info(
			"codec {} does not support channel layout {} so transcoding to {}",
			avcodec_get_name(codec_id),
			GetChannelLayoutString(channel_layout),
			GetChannelLayoutString(context->channel_layout));
	context->channels = av_get_channel_layout_nb_channels(context->channel_layout);
	context->time_base = AVRational{ 1, context->sample_rate };
	auto dict = options.release();
	auto ret = avcodec_open2(context.get(), nullptr, &dict);
	options.reset(dict);
	if (ret < 0)
		LOG_THROW(std::runtime_error, fmt::format("failed to open audio codec: {}", AVErrorString(ret)));
	avcodec_parameters_from_context(stream->codecpar, context.get());
	// note: this is only a hint, actual stream time_base can be different
	// avformat_write_header will set the final stream time_base
	// see https://ffmpeg.org/doxygen/trunk/structAVStream.html#a9db755451f14e2bf590d4b85d82b32e6
	stream->time_base = context->time_base;
	int nb_samples = (context->codec->capabilities & AV_CODEC_CAP_VARIABLE_FRAME_SIZE) ? 1000 : context->frame_size;
	LOG->debug("codec frame size is {}", nb_samples);
	dst_frame = CreateAudioFrame(context->sample_fmt, context->sample_rate, context->channel_layout, nb_samples);
	swr = CreateSwrContext(
		context->channel_layout, context->sample_fmt, context->sample_rate, // out
		channel_layout, sample_fmt, sample_rate); // in
	fifo = CreateAVAudioFifo(context->sample_fmt, context->channels, dst_frame->nb_samples);
	LOG_EXIT_METHOD;
}

void AudioStream::Transcode(const AVFramePtr& src_frame)
{
	LOG_ENTER_METHOD;
	// create buffer frame
	auto buf_frame = CreateAudioFrame(context->sample_fmt, context->sample_rate, context->channel_layout);
	// resample source frame to buffer frame
	int ret = swr_convert_frame(swr.get(), buf_frame.get(), src_frame.get());
	if (ret < 0)
		LOG_THROW(std::runtime_error, fmt::format("resampling error: {}", AVErrorString(ret)));
	// save buffer frame to fifo buffer
	int nb_written = av_audio_fifo_write(fifo.get(), reinterpret_cast<void**>(buf_frame->data), buf_frame->nb_samples);
	if (nb_written < 0)
		LOG_THROW(std::runtime_error, fmt::format("failed to write to audio buffer: {}", AVErrorString(nb_written)));
	if (nb_written != buf_frame->nb_samples)
		LOG->warn("expected {} samples to be written to audio buffer but wrote {}", buf_frame->nb_samples, nb_written);
	// read from fifo buffer in chunks of dst_frame->nb_samples, until no further chunks can be read
	while (av_audio_fifo_size(fifo.get()) >= dst_frame->nb_samples) {
		int nb_read = av_audio_fifo_read(fifo.get(), reinterpret_cast<void**>(dst_frame->data), dst_frame->nb_samples);
		if (nb_read < 0)
			LOG_THROW(std::runtime_error, fmt::format("audio buffer read error: {}", AVErrorString(nb_read)));
		if (nb_read != dst_frame->nb_samples)
			LOG->warn("expected {} samples from audio buffer but got {}", dst_frame->nb_samples, nb_read);
		Encode(dst_frame);
		dst_frame->pts += nb_read;
	}
	// flush buffer if needed
	if (!src_frame) {
		int nb_read = av_audio_fifo_read(fifo.get(), reinterpret_cast<void**>(dst_frame->data), dst_frame->nb_samples);
		if (nb_read < 0)
			LOG_THROW(std::runtime_error, fmt::format("audio buffer read error: {}", AVErrorString(nb_read)));
		dst_frame->nb_samples = nb_read;
		Encode(dst_frame);
		dst_frame->pts += nb_read;
		Encode(nullptr);
		int nb_lost = av_audio_fifo_size(fifo.get());
		if (nb_lost)
			LOG->warn("audio buffer not completely flushed, {} samples lost");
	}
	LOG_EXIT_METHOD;
}
