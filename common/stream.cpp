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

auto FindCodec(const AVCodecID& codec_id) {
	LOG_ENTER;
	auto codec = avcodec_find_encoder(codec_id);
	if (!codec)
		LOG_THROW(std::invalid_argument, fmt::format("failed find codec with id {}", codec_id));
	LOG_EXIT;
	return codec;
}

AVStreamPtr CreateAVStream(AVFormatContext& format_context, const AVCodec& codec) {
	LOG_ENTER;
	auto stream = avformat_new_stream(&format_context, &codec);
	if (!stream)
		LOG_THROW(std::runtime_error, fmt::format("failed to allocate stream for {} codec", codec.name));
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
		LOG_THROW(std::runtime_error, fmt::format("failed to allocate context for {} codec", codec.name));
	LOG_EXIT;
	return AVCodecContextPtr{ context };
}

void AVCodecContextDeleter::operator()(AVCodecContext* context) const {
	avcodec_free_context(&context);
}

AVFramePtr CreateAVFrame() {
	auto frame = av_frame_alloc();
	if (!frame)
		LOG_THROW(std::runtime_error, "failed to allocate frame");
	frame->pts = 0;
	return AVFramePtr{ frame };
}

void AVFrameDeleter::operator()(AVFrame* frame) const {
	av_frame_free(&frame);
}

Stream::Stream(std::shared_ptr<AVFormatContext>& format_context, AVCodecID codec_id)
	: owner{ format_context }
	, codec{ FindCodec(codec_id) }
	, stream{ CreateAVStream(*format_context, *codec) }
	, context{ CreateAVCodecContext(*codec) }
	, frame{ CreateAVFrame() }
{
}

// encode and write the given frame to the stream
// to flush the encoder, send a nullptr as frame
void Stream::Encode(const AVFramePtr& avframe)
{
	LOG_ENTER;
	AVPacket pkt;
	av_init_packet(&pkt);
	// send frame for encoding
	int ret_frame = avcodec_send_frame(context.get(), avframe.get());
	if (ret_frame < 0) {
		LOG->error("failed to send frame to encoder: {}", AVErrorString(ret_frame));
	}
	else {
		// lock the format context (we will need it to write the packets)
		auto format_context = owner.lock();
		// get next packet from encoder
		int ret_packet = avcodec_receive_packet(context.get(), &pkt);
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
			int ret_write = av_interleaved_write_frame(format_context.get(), &pkt);
			if (ret_write < 0) {
				LOG->error("failed to write packet to stream: {}", AVErrorString(ret_write));
			}
			// clean up reference
			av_packet_unref(&pkt);
			// get next packet from encoder
			ret_packet = avcodec_receive_packet(context.get(), &pkt);
		}
		if (ret_packet != AVERROR(EAGAIN) && (ret_packet != AVERROR_EOF)) {
			LOG->error("failed to receive packet from encoder: {}", AVErrorString(ret_packet));
		}
	}
	LOG_EXIT;
}

double Stream::Time() const
{
	LOG_ENTER;
	return frame->pts * av_q2d(context->time_base);
	LOG_EXIT;
}
