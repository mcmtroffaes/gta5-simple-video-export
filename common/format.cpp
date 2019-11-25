#include "format.h"

Format::Format(
	const std::filesystem::path& filename,
	AVCodecID vcodec, AVDictionaryPtr& voptions, int width, int height, const AVRational& frame_rate, AVPixelFormat pix_fmt,
	AVCodecID acodec, AVDictionaryPtr& aoptions, AVSampleFormat sample_fmt, int sample_rate, uint64_t channel_layout)
	: context{ CreateAVFormatContext(filename) }
	, vstream{ context, vcodec, voptions, width, height, frame_rate, pix_fmt }
	, astream{ context, acodec, aoptions, sample_fmt, sample_rate, channel_layout }
{
	LOG_ENTER_METHOD;
	// the ffmpeg API expects a utf8 encoded const char * for the filename
	auto u8_filename{ filename.u8string() };
	auto c_filename{ reinterpret_cast<const char*>(u8_filename.c_str()) };
	av_dump_format(context.get(), 0, c_filename, 1);
	// none of the above functions should set context to null, but just in case...
	if (!context)
		LOG_THROW(std::runtime_error, fmt::format("output context lost"));
	int ret = avio_open(&context->pb, c_filename, AVIO_FLAG_WRITE);
	if (ret < 0 || !context->pb)
		LOG_THROW(std::runtime_error, fmt::format("failed to open '{}' for writing: {}", filename.string(), AVErrorString(ret)));
	if (!(context->oformat->flags & AVFMT_NOFILE)) {
		ret = avformat_write_header(context.get(), NULL);
		if (ret < 0)
			LOG_THROW(std::runtime_error, fmt::format("failed to write header: {}", AVErrorString(ret)));
	}
	LOG_EXIT_METHOD;
}

void Format::Flush()
{
	LOG_ENTER_METHOD;
	vstream.Transcode(nullptr);
	astream.Transcode(nullptr);
	int ret = av_write_trailer(context.get());
	if (ret < 0)
		LOG_THROW(std::runtime_error, fmt::format("failed to write trailer: {}", AVErrorString(ret)));
	LOG_EXIT_METHOD;
}

