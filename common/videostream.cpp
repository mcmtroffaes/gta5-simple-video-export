#include "videostream.h"

extern "C" {
#include <libavutil/imgutils.h>
#include <libavutil/pixdesc.h>
#include <libswscale/swscale.h>
}

VideoStream::VideoStream(AVFormatContext* format_context, AVCodecID codec_id, int width, int height, const AVRational& frame_rate, AVPixelFormat pix_fmt)
	: Stream{ format_context, codec_id }, pix_fmt{ pix_fmt }
{
	LOG_ENTER;
	if (context) {
		if (context->codec && context->codec->type != AVMEDIA_TYPE_VIDEO) {
			LOG->warn("selected video codec {} does not support video", context->codec->name);
		}
		context->width = width;
		context->height = height;
		context->time_base = av_inv_q(frame_rate);
		int loss = 0;
		if (context->codec && context->codec->pix_fmts) {
			context->pix_fmt = avcodec_find_best_pix_fmt_of_list(context->codec->pix_fmts, pix_fmt, 0, &loss);
		}
		else {
			context->pix_fmt = pix_fmt;
		}
		if (context->pix_fmt != pix_fmt) {
			LOG->info(
				"pixel format {} not supported by codec so using {} instead",
				av_get_pix_fmt_name(pix_fmt), av_get_pix_fmt_name(context->pix_fmt));
		}
		if (loss) {
			LOG->warn(
				"pixel format conversion from {} to {} is lossy",
				av_get_pix_fmt_name(pix_fmt),
				av_get_pix_fmt_name(context->pix_fmt));
		}
		int ret = 0;
		ret = avcodec_open2(context, NULL, NULL);
		if (ret < 0) {
			LOG->error("failed to open video codec: {}", AVErrorString(ret));
		}
		if (stream) {
			stream->time_base = context->time_base;
			avcodec_parameters_from_context(stream->codecpar, context);
		}
		if (frame) {
			frame->width = width;
			frame->height = height;
			frame->format = context->pix_fmt;
			int ret = av_frame_get_buffer(frame, 32);
			if (ret < 0) {
				LOG->error("failed to allocate frame buffer");
			}
		}
	}
	LOG_EXIT;
}

void VideoStream::MakeFrame(uint8_t* ptr)
{
	LOG_ENTER;
	struct SwsContext* sws = sws_getContext(
		frame->width, frame->height, pix_fmt,
		frame->width, frame->height, context->pix_fmt,
		SWS_BICUBIC, nullptr, nullptr, nullptr);
	if (!sws) {
		LOG->error("failed to initialize pixel conversion context");
	}
	else {
		uint8_t* data[4];
		int linesize[4];
		av_image_fill_linesizes(linesize, pix_fmt, frame->width);
		av_image_fill_pointers(data, pix_fmt, frame->height, ptr, linesize);
		sws_scale(
			sws,
			data, linesize, 0, frame->height,
			frame->data, frame->linesize);
		sws_freeContext(sws);
	}
	LOG_EXIT;
}
