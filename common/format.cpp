#include "format.h"

Format::Format(const std::string& filename, AVCodecID vcodec, int width, int height, const AVRational& frame_rate, AVPixelFormat pix_fmt, AVCodecID acodec, AVSampleFormat sample_fmt, int sample_rate, uint64_t channel_layout)
	: context{ nullptr }, vstream{ nullptr }, astream{ nullptr }
{
	LOG_ENTER;
	int ret = 0;
	ret = avformat_alloc_output_context2(&context, NULL, NULL, filename.c_str());
	if (ret < 0) {
		LOG->error("failed to allocate output context for '{}': {}", filename, AVErrorString(ret));
	}
	if (context) {
		vstream.reset(new VideoStream(context, vcodec, width, height, frame_rate, pix_fmt));
		astream.reset(new AudioStream(context, acodec, sample_fmt, sample_rate, channel_layout));
		av_dump_format(context, 0, filename.c_str(), 1);
		ret = avio_open(&context->pb, filename.c_str(), AVIO_FLAG_WRITE);
		if (ret < 0) {
			LOG->error("failed to open '{}' for writing: {}", filename, AVErrorString(ret));
		}
	}
	if (context && context->pb) {
		if (!(context->oformat->flags & AVFMT_NOFILE)) {
			ret = avformat_write_header(context, NULL);
			if (ret < 0) {
				LOG->error("failed to write header: {}", AVErrorString(ret));
				avio_closep(&context->pb);
			}
		}
	}
	LOG_EXIT;
}

Format::~Format()
{
	LOG_ENTER;
	vstream = nullptr;
	astream = nullptr;
	if (context && context->pb) {
		int ret = av_write_trailer(context);
		if (ret < 0) {
			LOG->error("failed to write trailer: {}", AVErrorString(ret));
		}
	}
	if (context) {
		avio_closep(&context->pb);
		avformat_free_context(context);
	}
	LOG_EXIT;
}
