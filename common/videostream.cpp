#include "videostream.h"

extern "C" {
#include <libavutil/imgutils.h>
#include <libavutil/pixdesc.h>
}

AVFramePtr CreateVideoFrame(int width, int height, AVPixelFormat pix_fmt) {
	LOG_ENTER;
	auto frame = CreateAVFrame();
	frame->width = width;
	frame->height = height;
	frame->format = pix_fmt;
	int ret = av_frame_get_buffer(frame.get(), 0);
	if (ret < 0)
		LOG_THROW(std::runtime_error, fmt::format("failed to allocate frame buffer: {}", AVErrorString(ret)));	
	LOG_EXIT;
	return frame;
}

AVFramePtr CreateVideoFrame(int width, int height, AVPixelFormat pix_fmt, uint8_t* ptr) {
	LOG_ENTER;
	auto frame = CreateAVFrame();
	frame->width = width;
	frame->height = height;
	frame->format = pix_fmt;
	int ret = av_image_fill_linesizes(frame->linesize, pix_fmt, width);
	if (ret < 0)
		LOG_THROW(std::runtime_error, "failed to get image line sizes");
	ret = av_image_fill_pointers(frame->data, pix_fmt, height, ptr, frame->linesize);
	if (ret < 0)
		LOG_THROW(std::runtime_error, "failed to get image pointers");
	LOG_EXIT;
	return frame;
}

VideoStream::VideoStream(std::shared_ptr<AVFormatContext>& format_context, AVCodecID codec_id, AVDictionaryPtr& options, int width, int height, const AVRational& frame_rate, AVPixelFormat pix_fmt)
	: Stream{ format_context, codec_id }, pix_fmt{ pix_fmt }, dst_frame{ nullptr }
{
	LOG_ENTER_METHOD;
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
			"codec {} does not support pixel format {} so transcoding to {}",
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
	auto dict = options.release();
	auto ret = avcodec_open2(context.get(), nullptr, &dict);
	options.reset(dict);
	if (ret < 0)
		LOG_THROW(std::runtime_error, fmt::format("failed to open video codec: {}", AVErrorString(ret)));
	avcodec_parameters_from_context(stream->codecpar, context.get());
	// note: this is only a hint, actual stream time_base can be different
	// avformat_write_header will set the final stream time_base
	// see https://ffmpeg.org/doxygen/trunk/structAVStream.html#a9db755451f14e2bf590d4b85d82b32e6
	stream->time_base = context->time_base;
	dst_frame = CreateVideoFrame(width, height, context->pix_fmt);
	dst_frame->pts = 0;
	LOG_EXIT_METHOD;
}

void VideoStream::Transcode(const AVFramePtr& src_frame)
{
	LOG_ENTER_METHOD;
	// nullptr means flushing the encoder
	if (!src_frame) {
		Encode(nullptr);
		LOG_EXIT_METHOD;
		return;
	}
	// fill frame with data given in ptr
	// we use sws_scale to do this, this will also take care of any pixel format conversions
	auto sws = CreateSwsContext(
		src_frame->width, src_frame->height, static_cast<AVPixelFormat>(src_frame->format),
		dst_frame->width, dst_frame->height, static_cast<AVPixelFormat>(dst_frame->format),
		SWS_BICUBIC);
	sws_scale(
		sws.get(),
		src_frame->data, src_frame->linesize, 0, src_frame->height,
		dst_frame->data, dst_frame->linesize);
	// now encode the frame
	Encode(dst_frame);
	// update destination frame timestamp
	dst_frame->pts += 1;
	LOG_EXIT_METHOD;
}

