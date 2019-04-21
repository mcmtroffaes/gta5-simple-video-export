#include "stream.h"

extern "C" {
#include <libavutil/timestamp.h>
}

auto AVTsString(uint64_t ts) {
	char buffer[AV_TS_MAX_STRING_SIZE] = { 0 };
	av_ts_make_string(buffer, ts);
	return std::string(buffer);
}

auto AVTsTimeString(uint64_t ts, AVRational* tb) {
	char buffer[AV_TS_MAX_STRING_SIZE] = { 0 };
	av_ts_make_time_string(buffer, ts, tb);
	return std::string(buffer);
}

Stream::Stream(AVFormatContext* format_context, AVCodecID codec_id)
	: format_context{ format_context }, stream { nullptr }, context{ nullptr }, frame{ nullptr }
{
	LOG_ENTER;
	const AVCodec* codec = avcodec_find_encoder(codec_id);
	if (!codec) {
		LOG->error("failed to find encoder with codec id {}", codec_id);
	}
	else {
		stream = avformat_new_stream(format_context, codec);
		if (!stream) {
			LOG->error("failed to allocate stream for codec {}", codec->name);
		}
		context = avcodec_alloc_context3(codec);
		if (!context) {
			LOG->error("failed to allocate context for codec {}", codec->name);
		}
		frame = av_frame_alloc();
		if (!frame) {
			LOG->error("failed to allocate frame");
		}
		else {
			frame->pts = 0;
		}
	}
	LOG_EXIT;
}

void Stream::Encode()
{
	LOG_ENTER;
	// note: frame can be null (e.g. on flush)
	if (!context) { 
		LOG->error("failed to send frame to encoder: no codec context");
	}
	else if (!stream) {
		LOG->error("failed to send frame to encoder: no stream");
	}
	else {
		AVPacket pkt;
		av_init_packet(&pkt);
		// send frame for encoding
		int ret_frame = avcodec_send_frame(context, frame);
		if (ret_frame < 0) {
			LOG->error("failed to send frame to encoder: {}", AVErrorString(ret_frame));
		}
		else {
			// get next packet from encoder
			int ret_packet = avcodec_receive_packet(context, &pkt);
			// ret_packet == 0 denotes success, keep writing as long as we have success
			while (!ret_packet) {
				// we have to set the correct stream index
				pkt.stream_index = stream->index;
				// we need to rescale the packet timestamps from the context time base to the stream time base
				av_packet_rescale_ts(&pkt, context->time_base, stream->time_base);
				// process the frame
				AVRational* time_base = &stream->time_base;
				LOG->debug(
					"pts:{} pts_time:{} dts:{} dts_time:{} duration:{} duration_time:{} stream_index:{}",
					AVTsString(pkt.pts), AVTsTimeString(pkt.pts, time_base),
					AVTsString(pkt.dts), AVTsTimeString(pkt.dts, time_base),
					AVTsString(pkt.duration), AVTsTimeString(pkt.duration, time_base),
					pkt.stream_index);
				int ret_write = av_interleaved_write_frame(format_context, &pkt);
				if (ret_write < 0) {
					LOG->error("failed to write packet to stream: {}", AVErrorString(ret_write));
				}
				// clean up reference
				av_packet_unref(&pkt);
				// get next packet from encoder
				ret_packet = avcodec_receive_packet(context, &pkt);
			}
			if (ret_packet != AVERROR(EAGAIN) && (ret_packet != AVERROR_EOF)) {
				LOG->error("failed to receive packet from encoder: {}", AVErrorString(ret_packet));
			}
		}
	}
	LOG_EXIT;
}

double Stream::Time() const
{
	LOG_ENTER;
	if (frame && context) {
		return frame->pts * av_q2d(context->time_base);
	}
	else {
		return 0.0;
	}
	LOG_EXIT;
}

Stream::~Stream()
{
	LOG_ENTER;
	if (frame) {
		av_frame_free(&frame);
		// flush the encoder by sending an empty frame
		Encode();
	}
	if (context) {
		avcodec_free_context(&context);
	}
	stream = nullptr;
	LOG_EXIT;
}