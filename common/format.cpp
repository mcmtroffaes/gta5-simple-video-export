#include "format.h"

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
		astream->Transcode(nullptr);
	int ret = av_write_trailer(context.get());
	if (ret < 0)
		LOG_THROW(std::runtime_error, fmt::format("failed to write trailer: {}", AVErrorString(ret)));
}

