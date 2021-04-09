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

Stream::Stream(std::shared_ptr<AVFormatContext>& format_context, const AVCodec& codec)
	: owner{ format_context }
	, stream{ CreateAVStream(*format_context, codec) }
	, context{ CreateAVCodecContext(codec) }
{
	LOG_ENTER_METHOD;
	if (format_context->oformat->flags & AVFMT_GLOBALHEADER)
		context->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
	LOG_EXIT_METHOD;
}

// encode and write the given frame to the stream
// to flush the encoder, send a nullptr as frame
void Stream::Encode(const AVFramePtr& frame)
{
	LOG_ENTER_METHOD;
	auto pkt = CreateAVPacket();
	// send frame for encoding
	int ret_frame = avcodec_send_frame(context.get(), frame.get());
	if (ret_frame < 0)
		throw std::runtime_error(fmt::format("failed to send frame to encoder: {}", AVErrorString(ret_frame)));
	// lock the format context (we will need it to write the packets)
	auto format_context = owner.lock();
	if (!format_context)
		throw std::runtime_error("failed to lock format context");
	// get next packet from encoder
	int ret_packet = avcodec_receive_packet(context.get(), pkt.get());
	// ret_packet == 0 denotes success, keep writing as long as we have success
	while (!ret_packet) {
		// we have to set the correct stream index
		pkt->stream_index = stream->index;
		// we need to rescale the packet timestamps from the context time base to the stream time base
		av_packet_rescale_ts(pkt.get(), context->time_base, stream->time_base);
		// process the frame
		AVRational* time_base = &stream->time_base;
		LOG->debug(
			"pts:{} pts_time:{} dts:{} dts_time:{} duration:{} duration_time:{} stream_index:{}",
			AVTsString(pkt->pts), AVTsTimeString(pkt->pts, time_base),
			AVTsString(pkt->dts), AVTsTimeString(pkt->dts, time_base),
			AVTsString(pkt->duration), AVTsTimeString(pkt->duration, time_base),
			pkt->stream_index);
		int ret_write = av_interleaved_write_frame(format_context.get(), pkt.get());
		if (ret_write < 0)
			throw std::runtime_error(fmt::format("failed to write packet to stream: {}", AVErrorString(ret_write)));
		// clean up reference
		av_packet_unref(pkt.get());
		// get next packet from encoder
		ret_packet = avcodec_receive_packet(context.get(), pkt.get());
	}
	if (ret_packet != AVERROR(EAGAIN) && (ret_packet != AVERROR_EOF))
		throw std::runtime_error(fmt::format("failed to receive packet from encoder: {}", AVErrorString(ret_packet)));
	LOG_EXIT_METHOD;
}
