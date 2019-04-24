#include "videostream.h"

extern "C" {
#include <libavutil/imgutils.h>
#include <libavutil/pixdesc.h>
#include <libswscale/swscale.h>
}

VideoStream::VideoStream(std::shared_ptr<AVFormatContext>& format_context, AVCodecID codec_id, int width, int height, const AVRational& frame_rate, AVPixelFormat pix_fmt)
	: Stream{ format_context, codec_id }, pix_fmt{ pix_fmt }
{
	LOG_ENTER;
	if (context->codec->type != AVMEDIA_TYPE_VIDEO)
		LOG_THROW(std::invalid_argument, fmt::format("selected video codec {} does not support video", context->codec->name));
	context->width = width;
	context->height = height;
	context->time_base = av_inv_q(frame_rate);
	int loss = 0;
	if (context->codec->pix_fmts) {
		context->pix_fmt = avcodec_find_best_pix_fmt_of_list(context->codec->pix_fmts, pix_fmt, 0, &loss);
	}
	else {
		context->pix_fmt = pix_fmt;
	}
	if (context->pix_fmt != pix_fmt) {
		LOG->info(
			"codec {} does not support pixel format {} so using {}",
			avcodec_get_name(codec_id),
			av_get_pix_fmt_name(pix_fmt),
			av_get_pix_fmt_name(context->pix_fmt));
	}
	if (loss) {
		LOG->warn(
			"pixel format conversion from {} to {} is lossy",
			av_get_pix_fmt_name(pix_fmt),
			av_get_pix_fmt_name(context->pix_fmt));
	}
	int ret = avcodec_open2(context.get(), NULL, NULL);
	if (ret < 0)
		LOG_THROW(std::runtime_error, fmt::format("failed to open video codec: {}", AVErrorString(ret)));
	avcodec_parameters_from_context(stream->codecpar, context.get());
	// note: this is only a hint, actual stream time_base can be different
	// avformat_write_header will set the final stream time_base
	// see https://ffmpeg.org/doxygen/trunk/structAVStream.html#a9db755451f14e2bf590d4b85d82b32e6
	stream->time_base = context->time_base;
	frame->width = width;
	frame->height = height;
	frame->format = context->pix_fmt;
	ret = av_frame_get_buffer(frame.get(), 0);
	if (ret < 0)
		LOG_THROW(std::runtime_error, fmt::format("failed to allocate frame buffer: {}", AVErrorString(ret)));
	LOG_EXIT;
}

void VideoStream::Transcode(uint8_t* ptr)
{
	LOG_ENTER;
	// we support passing a null ptr as a way of flushing the transcoder
	if (!ptr) {
		Stream::Encode(nullptr);
		LOG_EXIT;
		return;
	}
	// fill frame with data given in ptr
	// we use sws_scale to do this, this will also take care of any pixel format conversions
	SwsContext* sws = sws_getContext(
		frame->width, frame->height, pix_fmt,
		frame->width, frame->height, context->pix_fmt,
		SWS_BICUBIC, nullptr, nullptr, nullptr);
	if (!sws)
		LOG_THROW(std::runtime_error, "failed to initialize pixel conversion context");
	uint8_t* data[4];
	int linesize[4];
	int ret = av_image_fill_linesizes(linesize, pix_fmt, frame->width);
	if (ret < 0)
		LOG_THROW(std::runtime_error, "failed to get image line sizes");
	ret = av_image_fill_pointers(data, pix_fmt, frame->height, ptr, linesize);
	if (ret < 0)
		LOG_THROW(std::runtime_error, "failed to get image pointers");
	sws_scale(
		sws,
		data, linesize, 0, frame->height,
		frame->data, frame->linesize);
	sws_freeContext(sws);
	// now encode the frame
	Encode(frame);
	// frame was sent, so update its presentation time stamp, for next encoding call
	frame->pts += 1;
	LOG_EXIT;
}
