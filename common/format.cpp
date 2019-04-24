#include "format.h"

struct AVFormatContextDeleter {
	void operator()(AVFormatContext* context) const {
		LOG_ENTER;
		if (context) avio_closep(&context->pb);
		avformat_free_context(context);
		LOG_EXIT;
	}
};

std::shared_ptr<AVFormatContext> CreateAVFormatContext(const std::string& filename) {
	LOG_ENTER;
	AVFormatContext* context{ nullptr };
	int ret{ avformat_alloc_output_context2(&context, NULL, NULL, filename.c_str()) };
	if (ret < 0 || !context)
		LOG_THROW(std::runtime_error, fmt::format("failed to allocate output context for '{}': {}", filename, AVErrorString(ret)));
	LOG_EXIT;
	return std::shared_ptr<AVFormatContext>(context, AVFormatContextDeleter());
}

Format::Format(const std::string& filename, AVCodecID vcodec, int width, int height, const AVRational& frame_rate, AVPixelFormat pix_fmt, AVCodecID acodec, AVSampleFormat sample_fmt, int sample_rate, uint64_t channel_layout)
	: context{ CreateAVFormatContext(filename) }
	, vstream{ new VideoStream(context, vcodec, width, height, frame_rate, pix_fmt) }
	, astream{ new AudioStream(context, acodec, sample_fmt, sample_rate, channel_layout) }
{
	LOG_ENTER;
	av_dump_format(context.get(), 0, filename.c_str(), 1);
	// none of the above functions should set context to null, but just in case...
	if (!context)
		LOG_THROW(std::runtime_error, fmt::format("output context lost"));
	int ret = avio_open(&context->pb, filename.c_str(), AVIO_FLAG_WRITE);
	if (ret < 0 || !context->pb)
		LOG_THROW(std::runtime_error, fmt::format("failed to open '{}' for writing: {}", filename, AVErrorString(ret)));
	if (!(context->oformat->flags & AVFMT_NOFILE)) {
		ret = avformat_write_header(context.get(), NULL);
		if (ret < 0)
			LOG_THROW(std::runtime_error, fmt::format("failed to write header: {}", AVErrorString(ret)));
	}
	LOG_EXIT;
}

void Format::Flush()
{
	if (vstream)
		vstream->Transcode(nullptr);
	if (astream)
		astream->Transcode(nullptr, 0);
	int ret = av_write_trailer(context.get());
	if (ret < 0)
		LOG_THROW(std::runtime_error, fmt::format("failed to write trailer: {}", AVErrorString(ret)));
}

