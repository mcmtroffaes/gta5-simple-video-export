#include "avcreate.h"
#include "logger.h"

AVFormatContextPtr CreateAVFormatContext(const std::string& filename) {
	LOG_ENTER;
	AVFormatContext* context{ nullptr };
	int ret{ avformat_alloc_output_context2(&context, NULL, NULL, filename.c_str()) };
	if (ret < 0)
		LOG_THROW(std::runtime_error, fmt::format("failed to allocate output context for '{}': {}", filename, AVErrorString(ret)));
	if (!context)
		LOG_THROW(std::runtime_error, fmt::format("failed to allocate output context for '{}'", filename));
	LOG_EXIT;
	return AVFormatContextPtr{ context };
}

void AVFormatContextDeleter::operator()(AVFormatContext* context) const {
	LOG_ENTER;
	if (context) avio_closep(&context->pb);
	avformat_free_context(context);
	LOG_EXIT;
}

AVCodecPtr CreateAVCodec(const AVCodecID& codec_id) {
	LOG_ENTER;
	auto codec = avcodec_find_encoder(codec_id);
	if (!codec)
		LOG_THROW(std::invalid_argument, fmt::format("failed find codec with id {}", codec_id));
	LOG_EXIT;
	return codec;
}

AVStreamPtr CreateAVStream(AVFormatContext& format_context, const AVCodec& codec) {
	LOG_ENTER;
	auto stream = avformat_new_stream(&format_context, &codec);
	if (!stream)
		LOG_THROW(std::runtime_error, fmt::format("failed to allocate stream for {} codec", codec.name));
	LOG_EXIT;
	return AVStreamPtr{ stream };
}

void AVStreamDeleter::operator()(AVStream* stream) const {
	// do nothing: memory is freed by the owning format context
}

AVCodecContextPtr CreateAVCodecContext(const AVCodec& codec) {
	LOG_ENTER;
	auto context = avcodec_alloc_context3(&codec);
	if (!context)
		LOG_THROW(std::runtime_error, fmt::format("failed to allocate context for {} codec", codec.name));
	LOG_EXIT;
	return AVCodecContextPtr{ context };
}

void AVCodecContextDeleter::operator()(AVCodecContext* context) const {
	LOG_ENTER;
	avcodec_free_context(&context);
	LOG_EXIT;
}

AVFramePtr CreateAVFrame() {
	LOG_ENTER;
	auto frame = av_frame_alloc();
	if (!frame)
		LOG_THROW(std::runtime_error, "failed to allocate frame");
	frame->pts = 0;
	LOG_EXIT;
	return AVFramePtr{ frame };
}

void AVFrameDeleter::operator()(AVFrame* frame) const {
	LOG_ENTER;
	av_frame_free(&frame);
	LOG_EXIT;
}

SwrContextPtr CreateSwrContext(
	uint64_t out_channel_layout, AVSampleFormat out_sample_fmt, int out_sample_rate,
	uint64_t in_channel_layout, AVSampleFormat in_sample_fmt, int in_sample_rate)
{
	LOG_ENTER;
	auto swr = swr_alloc_set_opts(
		NULL,
		out_channel_layout, out_sample_fmt, out_sample_rate,
		in_channel_layout, in_sample_fmt, in_sample_rate,
		0, NULL);
	if (!swr)
		LOG_THROW(std::runtime_error, "failed to allocate resampling context");
	int ret = swr_init(swr);
	if (ret < 0)
		LOG_THROW(std::runtime_error, fmt::format("failed to initialize resampling context: {}", AVErrorString(ret)));
	LOG_EXIT;
	return SwrContextPtr{ swr };
}

void SwrContextDeleter::operator()(SwrContext* swr) const {
	LOG_ENTER;
	swr_free(&swr);
	LOG_EXIT;
}

SwsContextPtr CreateSwsContext(int srcW, int srcH, AVPixelFormat srcFormat, int dstW, int dstH, AVPixelFormat dstFormat, int flags) {
	LOG_ENTER;
	auto sws = sws_getContext(
		srcW, srcH, srcFormat,
		dstW, dstH, dstFormat,
		flags, nullptr, nullptr, nullptr);
	if (!sws)
		LOG_THROW(std::runtime_error, "failed to initialize pixel conversion context");
	LOG_EXIT;
	return SwsContextPtr{ sws };
}

void SwsContextDeleter::operator()(SwsContext* sws) const {
	LOG_ENTER;
	sws_freeContext(sws);
	LOG_EXIT;
}

AVAudioFifoPtr CreateAVAudioFifo(AVSampleFormat sample_fmt, int channels, int nb_samples) {
	LOG_ENTER;
	auto fifo = av_audio_fifo_alloc(sample_fmt, channels, nb_samples);
	if (!fifo)
		LOG_THROW(std::runtime_error, "failed to allocate audio buffer");
	LOG_EXIT;
	return AVAudioFifoPtr{ fifo };
}

void AVAudioFifoDeleter::operator()(AVAudioFifo* fifo) const {
	LOG_ENTER;
	av_audio_fifo_free(fifo);
	LOG_EXIT;
}
