#include "avcreate.h"
#include "logger.h"

AVFormatContextPtr CreateAVFormatContext(const std::filesystem::path& filename) {
	LOG_ENTER;
	AVFormatContext* context{ nullptr };
	auto u8_filename{ filename.u8string() };
	auto c_filename{ reinterpret_cast<const char*>(u8_filename.c_str()) };
	auto ret{ avformat_alloc_output_context2(&context, nullptr, nullptr, c_filename) };
	if (ret < 0)
		throw std::runtime_error(fmt::format("failed to allocate output context for '{}': {}", c_filename, AVErrorString(ret)));
	if (!context)
		throw std::runtime_error(fmt::format("failed to allocate output context for '{}'", c_filename));
	LOG_EXIT;
	return AVFormatContextPtr{ context };
}

void AVFormatContextDeleter::operator()(AVFormatContext* context) const {
	LOG_ENTER_METHOD;
	if (context) avio_closep(&context->pb);
	avformat_free_context(context);
	LOG_EXIT_METHOD;
}

AVCodecPtr CreateAVCodec(const std::string& name, const AVCodecID& fallback) {
	LOG_ENTER;
	auto codec = avcodec_find_encoder_by_name(name.c_str());
	if (!codec) {
		LOG->warn("failed to find codec {}", name);
		codec = avcodec_find_encoder(fallback);
		if (!codec)
			throw std::invalid_argument(fmt::format("failed find fallback codec with id {}", (int)fallback));
		LOG->warn("codec {} used as fallback", codec->name);
	}
	LOG_EXIT;
	return codec;
}

AVStreamPtr CreateAVStream(AVFormatContext& format_context, const AVCodec& codec) {
	LOG_ENTER;
	auto stream = avformat_new_stream(&format_context, &codec);
	if (!stream)
		throw std::runtime_error(fmt::format("failed to allocate stream for {} codec", codec.name));
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
		throw std::runtime_error(fmt::format("failed to allocate context for {} codec", codec.name));
	LOG_EXIT;
	return AVCodecContextPtr{ context };
}

void AVCodecContextDeleter::operator()(AVCodecContext* context) const {
	LOG_ENTER_METHOD;
	avcodec_free_context(&context);
	LOG_EXIT_METHOD;
}

AVFramePtr CreateAVFrame() {
	LOG_ENTER;
	auto frame = av_frame_alloc();
	if (!frame)
		throw std::runtime_error("failed to allocate frame");
	frame->pts = 0;
	LOG_EXIT;
	return AVFramePtr{ frame };
}

void AVFrameDeleter::operator()(AVFrame* frame) const {
	LOG_ENTER_METHOD;
	av_frame_free(&frame);
	LOG_EXIT_METHOD;
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
		throw std::runtime_error("failed to allocate resampling context");
	int ret = swr_init(swr);
	if (ret < 0)
		throw std::runtime_error(fmt::format("failed to initialize resampling context: {}", AVErrorString(ret)));
	LOG_EXIT;
	return SwrContextPtr{ swr };
}

void SwrContextDeleter::operator()(SwrContext* swr) const {
	LOG_ENTER_METHOD;
	swr_free(&swr);
	LOG_EXIT_METHOD;
}

SwsContextPtr CreateSwsContext(int srcW, int srcH, AVPixelFormat srcFormat, int dstW, int dstH, AVPixelFormat dstFormat, int flags) {
	LOG_ENTER;
	auto sws = sws_getContext(
		srcW, srcH, srcFormat,
		dstW, dstH, dstFormat,
		flags, nullptr, nullptr, nullptr);
	if (!sws)
		throw std::runtime_error("failed to initialize pixel conversion context");
	LOG_EXIT;
	return SwsContextPtr{ sws };
}

void SwsContextDeleter::operator()(SwsContext* sws) const {
	LOG_ENTER_METHOD;
	sws_freeContext(sws);
	LOG_EXIT_METHOD;
}

AVAudioFifoPtr CreateAVAudioFifo(AVSampleFormat sample_fmt, int channels, int nb_samples) {
	LOG_ENTER;
	auto fifo = av_audio_fifo_alloc(sample_fmt, channels, nb_samples);
	if (!fifo)
		throw std::runtime_error("failed to allocate audio buffer");
	LOG_EXIT;
	return AVAudioFifoPtr{ fifo };
}

void AVAudioFifoDeleter::operator()(AVAudioFifo* fifo) const {
	LOG_ENTER_METHOD;
	av_audio_fifo_free(fifo);
	LOG_EXIT_METHOD;
}

AVDictionaryPtr CreateAVDictionary(const std::string& options, const std::string& key_val_sep, const std::string& pairs_sep)
{
	LOG_ENTER;
	AVDictionary* dict{};
	auto ret = av_dict_parse_string(&dict, options.c_str(), key_val_sep.c_str(), pairs_sep.c_str(), 0);
	if (ret < 0)
		LOG->error("failed to parse dictionary {}", options);
	LOG_EXIT;
	return AVDictionaryPtr{ dict };
}

void AVDictionaryDeleter::operator()(AVDictionary* dict) const
{
	LOG_ENTER_METHOD;
	av_dict_free(&dict);
	LOG_EXIT_METHOD;
}

AVPacketPtr CreateAVPacket()
{
	LOG_ENTER;
	auto pkt = av_packet_alloc();
	if (!pkt)
		throw std::runtime_error("failed to allocate packet");
	LOG_EXIT;
	return AVPacketPtr{ pkt };
}

void AVPacketDeleter::operator()(AVPacket* pkt) const
{
	LOG_ENTER_METHOD;
	av_packet_free(&pkt);
	LOG_EXIT_METHOD;
}
