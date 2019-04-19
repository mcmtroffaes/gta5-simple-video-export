#include <codecvt>
#include <iostream>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/pixdesc.h>
#include <libavutil/timestamp.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
}

#include "settings.h"

#pragma comment(lib, "common.lib")

std::shared_ptr<spdlog::logger> logger = nullptr;
std::unique_ptr<Settings> settings = nullptr;
std::mutex av_log_mutex;

std::string wstring_to_utf8(const std::wstring& str)
{
	std::wstring_convert<std::codecvt_utf8<wchar_t> > myconv;
	return myconv.to_bytes(str);
}

std::wstring wstring_from_utf8(const std::string& str)
{
	std::wstring_convert<std::codecvt_utf8<wchar_t> > myconv;
	return myconv.from_bytes(str);
}

auto spdlog_av_level(spdlog::level::level_enum level) {
	switch (level) {
	case spdlog::level::trace:
		return AV_LOG_TRACE;
	case spdlog::level::debug:
		return AV_LOG_DEBUG;
	case spdlog::level::info:
		return AV_LOG_INFO;
	case spdlog::level::warn:
		return AV_LOG_WARNING;
	case spdlog::level::err:
		return AV_LOG_ERROR;
	case spdlog::level::critical:
		return AV_LOG_FATAL;
	case spdlog::level::off:
		return AV_LOG_QUIET;
	default:
		return AV_LOG_INFO;
	}
}

auto av_spdlog_level(int level) {
	switch (level) {
	case AV_LOG_TRACE:
		return spdlog::level::trace;
	case AV_LOG_DEBUG:
	case AV_LOG_VERBOSE:
		return spdlog::level::debug;
	case AV_LOG_INFO:
		return spdlog::level::info;
	case AV_LOG_WARNING:
		return spdlog::level::warn;
	case AV_LOG_ERROR:
		return spdlog::level::err;
	case AV_LOG_FATAL:
	case AV_LOG_PANIC:
		return spdlog::level::critical;
	case AV_LOG_QUIET:
		return spdlog::level::off;
	default:
		return spdlog::level::info;
	}
}

void av_log_callback(void* avcl, int level, const char* fmt, va_list vl)
{
	// each thread should have its own character buffer
	thread_local char line[256] = { 0 };
	thread_local int pos = 0;

	std::lock_guard<std::mutex> lock(av_log_mutex);
	int print_prefix = 1;
	int remain = sizeof(line) - pos;
	if (remain > 0) {
		int ret = av_log_format_line2(avcl, level, fmt, vl, line + pos, remain, &print_prefix);
		if (ret >= 0) {
			pos += (ret <= remain) ? ret : remain;
		}
		else {
			// log at the specified level rather than error level to avoid spamming the log
			LOG->log(av_spdlog_level(level), "failed to format av_log message '{}'", fmt);
		}
	}
	// only write log message on newline
	size_t i = strlen(fmt);
	if ((i > 0) && (fmt[i - 1] == '\n')) {
		// remove newline (spdlog adds a newline automatically)
		if ((pos > 0) && (line[pos - 1] == '\n')) {
			line[pos - 1] = '\0';
		}
		LOG->log(av_spdlog_level(level), line);
		pos = 0;
		*line = '\0';
	}
}

std::wstring av2_error_string(int errnum) {
	char buffer[AV_ERROR_MAX_STRING_SIZE] = { 0 };
	av_make_error_string(buffer, sizeof(buffer), errnum);
	return wstring_from_utf8(std::string(buffer));
}

std::wstring av2_ts_string(uint64_t ts) {
	char buffer[AV_TS_MAX_STRING_SIZE] = { 0 };
	av_ts_make_string(buffer, ts);
	return wstring_from_utf8(std::string(buffer));
}

std::wstring av2_ts_time_string(uint64_t ts, AVRational *tb) {
	char buffer[AV_TS_MAX_STRING_SIZE] = { 0 };
	av_ts_make_time_string(buffer, ts, tb);
	return wstring_from_utf8(std::string(buffer));
}

class ProcessPacket {
private:
	AVFormatContext* format_context;
public:
	ProcessPacket(AVFormatContext& format_context)
		: format_context{ &format_context }
	{
	};
	void operator()(AVPacket& pkt) {
		LOG_ENTER;
		AVRational* time_base = &format_context->streams[pkt.stream_index]->time_base;
		LOG->trace(
			L"pts:{} pts_time:{} dts:{} dts_time:{} duration:{} duration_time:{} stream_index:{}",
			av2_ts_string(pkt.pts), av2_ts_time_string(pkt.pts, time_base),
			av2_ts_string(pkt.dts), av2_ts_time_string(pkt.dts, time_base),
			av2_ts_string(pkt.duration), av2_ts_time_string(pkt.duration, time_base),
			pkt.stream_index);
		int ret_write = av_interleaved_write_frame(format_context, &pkt);
		if (ret_write < 0) {
			LOG->error(L"failed to write packet to stream: {}", av2_error_string(ret_write));
		}
		LOG_EXIT;
	}
};

class Stream {
public:
	AVStream* stream;
	AVCodecContext* context;
	AVFrame* frame;

	Stream(AVFormatContext& format_context, AVCodecID codec_id) :
		stream{ nullptr }, context{ nullptr }, frame{ nullptr }
	{
		LOG_ENTER;
		const AVCodec* codec = avcodec_find_encoder(codec_id);
		if (!codec) {
			LOG->error("failed to find encoder with codec id {}", codec_id);
		}
		else {
			stream = avformat_new_stream(&format_context, codec);
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
		}
		LOG_EXIT;
	}

	void ProcessFrame(ProcessPacket& process_packet) {
		LOG_ENTER;
		AVPacket pkt;
		if (!context || !stream) // frame can be null (e.g. on flush)
			return;
		av_init_packet(&pkt);
		int ret_frame = avcodec_send_frame(context, frame);
		if (ret_frame < 0) {
			LOG->error(L"failed to send frame to encoder: {}", av2_error_string(ret_frame));
		}
		else {
			int ret_packet = avcodec_receive_packet(context, &pkt);
			while (!ret_packet) { // ret_packet == 0 denotes success, keep writing as long as we have success
				// we have to set the correct stream index
				pkt.stream_index = stream->index;
				// we need to rescale the packet from the context time base to the stream time base
				av_packet_rescale_ts(&pkt, context->time_base, stream->time_base);
				process_packet(pkt);
				av_packet_unref(&pkt);
				ret_packet = avcodec_receive_packet(context, &pkt);
			}
			if (frame) {
				if (ret_packet != AVERROR(EAGAIN)) {
					LOG->error(L"failed to receive packet from encoder: {}", av2_error_string(ret_packet));
				}
			}
			else {
				if (ret_packet != AVERROR_EOF) {
					LOG->error(L"failed to receive final packet from encoder: {}", av2_error_string(ret_packet));
				}
			}
		}
		LOG_EXIT;
	}

	void Flush(ProcessPacket& process_packet) {
		LOG_ENTER;
		if (frame) {
			av_frame_free(&frame);
			ProcessFrame(process_packet);
		}
		LOG_EXIT;
	}

	~Stream() {
		LOG_ENTER;
		if (frame) {
			av_frame_free(&frame);
		}
		if (context) {
			avcodec_free_context(&context);
		}
		stream = nullptr;
		LOG_EXIT;
	}
};

void FillFrameNV12(AVFrame* f, AVRational& time_base) {
	LOG_ENTER;
	if (f->format != AV_PIX_FMT_NV12) {
		LOG->error("wrong pixel format for filling frame");
		return;
	}
	auto t = f->pts * av_q2d(time_base);
	for (int y = 0; y < f->height; y++)
		for (int x = 0; x < f->width; x++)
			f->data[0][y * f->linesize[0] + x] = (uint8_t)(x + y + t * 30);
	for (int y = 0; y < f->height / 2; y++) {
		for (int x = 0; x < f->width / 2; x++) {
			f->data[1][y * f->linesize[1] + 2 * x] = (uint8_t)(128 + y + t * 20);
			f->data[1][y * f->linesize[1] + 2 * x + 1] = (uint8_t)(64 + x + t * 50);
		}
	}
	LOG_EXIT;
}

class VideoStream : public Stream {
public:
	const AVPixelFormat pix_fmt;
	AVFrame* tmp_frame;

	VideoStream(
		AVFormatContext& format_context, AVCodecID codec_id,
		int width, int height,
		int framerate_numerator, int framerate_denominator,
		AVPixelFormat pix_fmt)
		: Stream{ format_context, codec_id }, pix_fmt{ pix_fmt }, tmp_frame{ nullptr }
	{
		LOG_ENTER;
		if (context) {
			if (context->codec && context->codec->type != AVMEDIA_TYPE_VIDEO) {
				LOG->warn("selected video codec {} does not support video", context->codec->name);
			}
			context->width = width;
			context->height = height;
			context->time_base = AVRational{ framerate_denominator, framerate_numerator };
			int loss = 0;
			if (context->codec && context->codec->pix_fmts) {
				context->pix_fmt = avcodec_find_best_pix_fmt_of_list(context->codec->pix_fmts, pix_fmt, 0, &loss);
			}
			else {
				context->pix_fmt = pix_fmt;
			}
			if (context->pix_fmt != pix_fmt) {
				LOG->info("pixel format {} not supported by codec", av_get_pix_fmt_name(pix_fmt));
				LOG->info("using pixel format {} instead", av_get_pix_fmt_name(context->pix_fmt));
				tmp_frame = av_frame_alloc();
				if (!tmp_frame) {
					LOG->error("failed to allocate conversion frame");
				}
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
				LOG->error(L"failed to open video codec: {}", av2_error_string(ret));
			}
			if (stream) {
				stream->time_base = context->time_base;
				avcodec_parameters_from_context(stream->codecpar, context);
			}
			if (frame) {
				frame->width = width;
				frame->height = height;
				frame->format = context->pix_fmt;
				frame->pts = 0;
				int ret = av_frame_get_buffer(frame, 32);
				if (ret < 0) {
					LOG->error("failed to allocate frame buffer");
				}
			}
			if (tmp_frame) {
				tmp_frame->width = width;
				tmp_frame->height = height;
				tmp_frame->format = pix_fmt;
				tmp_frame->pts = 0;
				int ret = av_frame_get_buffer(tmp_frame, 32);
				if (ret < 0) {
					LOG->error("failed to allocate frame buffer");
				}
			}
		}
		LOG_EXIT;
	}

	// fill frame with NV12 data
	void NextFrame() {
		LOG_ENTER;
		if (frame) {
			if (tmp_frame) {
				FillFrameNV12(tmp_frame, context->time_base);
				struct SwsContext* sws = sws_getContext(
					tmp_frame->width, tmp_frame->height, pix_fmt,
					frame->width, frame->height, context->pix_fmt,
					SWS_BICUBIC, nullptr, nullptr, nullptr);
				if (!sws) {
					LOG->error("failed to initialize pixel conversion context");
				}
				else {
					sws_scale(
						sws,
						tmp_frame->data, tmp_frame->linesize, 0, tmp_frame->height,
						frame->data, frame->linesize);
					sws_freeContext(sws);
				}
				tmp_frame->pts += 1;
				frame->pts += 1;
			}
			else {
				FillFrameNV12(frame, context->time_base);
				frame->pts += 1;
			}
		}
		LOG_EXIT;
	}
};

AVSampleFormat find_best_sample_fmt_of_list(const AVSampleFormat* sample_fmts, AVSampleFormat sample_fmt) {
	if (!sample_fmts) {
		return sample_fmt;
	}
	for (int i = 0; sample_fmts[i]; i++) {
		if (sample_fmts[i] == sample_fmt) {
			return sample_fmt;
		}
	}
	return sample_fmts[0];
}

class AudioStream : public Stream {
public:
	const AVSampleFormat sample_fmt;
	AVFrame* tmp_frame;

	AudioStream(
		AVFormatContext& format_context, AVCodecID codec_id,
		AVSampleFormat sample_fmt, int sample_rate, uint64_t channel_layout)
		: Stream{ format_context, codec_id }, sample_fmt{ sample_fmt }, tmp_frame{ nullptr }
	{
		LOG_ENTER;
		if (context && context->codec_type != AVMEDIA_TYPE_AUDIO) {
			LOG->warn("selected audio codec does not support audio");
			avcodec_free_context(&context);
		}
		if (context) {
			if (context->codec) {
				context->sample_fmt = find_best_sample_fmt_of_list(context->codec->sample_fmts, sample_fmt);
			}
			else {
				context->sample_fmt = sample_fmt;
			}
			if (context->sample_fmt != sample_fmt) {
				LOG->info("sample format {} not supported by codec", av_get_sample_fmt_name(sample_fmt));
				LOG->info("using sample format {} instead", av_get_sample_fmt_name(context->sample_fmt));
			}
			context->sample_rate = sample_rate;
			context->channel_layout = channel_layout;
			context->channels = av_get_channel_layout_nb_channels(channel_layout);
			context->time_base = AVRational{ 1, sample_rate };
			int ret = 0;
			ret = avcodec_open2(context, NULL, NULL);
			if (ret < 0) {
				LOG->error(L"failed to open audio codec: {}", av2_error_string(ret));
			}
			if (stream) {
				avcodec_parameters_from_context(stream->codecpar, context);
				// note: this is only a hint, actual stream time_base can be different
				// avformat_write_header will set the final stream time_base
				// see https://ffmpeg.org/doxygen/trunk/structAVStream.html#a9db755451f14e2bf590d4b85d82b32e6
				stream->time_base = context->time_base;
			}
			if (frame && context->codec) {
				frame->format = sample_fmt;
				frame->sample_rate = sample_rate;
				frame->channel_layout = channel_layout;
				frame->nb_samples = (context->codec->capabilities & AV_CODEC_CAP_VARIABLE_FRAME_SIZE) ? 1000 : context->frame_size;
				frame->pts = 0;
				LOG->debug("audio buffer size is {}", frame->nb_samples);
				int ret = av_frame_get_buffer(frame, 32);
				if (ret < 0) {
					LOG->error("failed to allocate frame buffer");
				}
			}
		}
		LOG_EXIT;
	}

	void NextFrame() {
		const auto freq1 = 220.0;
		const auto freq2 = 220.0 * 5.0 / 4.0; // perfect third
		const auto delta = 0.5;
		static uint64_t sample_count = 0;
		LOG_ENTER;
		int16_t* q = (int16_t*)frame->data[0];
		for (int j = 0; j < frame->nb_samples; j++) {
			auto two_pi_time = (2.0 * M_PI * (sample_count + j)) / frame->sample_rate;
			int v = (int)(10000 * sin(freq1 * two_pi_time + delta * sin(freq2 * two_pi_time)));
			for (int i = 0; i < frame->channels; i++) {
				*q++ = v;
			}
		}
		frame->pts = sample_count;
		sample_count += frame->nb_samples;
		LOG_EXIT;
	}

};

class Format {
public:
	AVFormatContext* context;
	std::unique_ptr<VideoStream> vstream;
	std::unique_ptr<AudioStream> astream;
	std::unique_ptr<ProcessPacket> process_packet;

	Format() : context{ nullptr }, vstream{ nullptr }, astream{ nullptr }
	{
		LOG_ENTER;
		std::wstring base;
		auto exportsec = settings->GetSec(L"export");
		settings->GetVar(exportsec, L"base", base);
		std::wstring filename{ base + L".mkv" };
		std::string ufilename{ wstring_to_utf8(filename) };
		int ret = 0;
		ret = avformat_alloc_output_context2(&context, NULL, NULL, ufilename.c_str());
		if (ret < 0) {
			LOG->error(L"failed to allocate output context for '{}': {}", filename, av2_error_string(ret));
		}
		if (context) {
			vstream.reset(new VideoStream(*context, AV_CODEC_ID_FFV1, 1920, 1080, 30000, 1001, AV_PIX_FMT_NV12));
			astream.reset(new AudioStream(*context, AV_CODEC_ID_FLAC, AV_SAMPLE_FMT_S16, 44100, AV_CH_LAYOUT_STEREO));
			process_packet.reset(new ProcessPacket(*context));
			av_dump_format(context, 0, ufilename.c_str(), 1);
			ret = avio_open(&context->pb, ufilename.c_str(), AVIO_FLAG_WRITE);
			if (ret < 0) {
				LOG->error(L"failed to open '{}' for writing: {}", filename, av2_error_string(ret));
			}
		}
		if (context && context->pb) {
			if (!(context->oformat->flags & AVFMT_NOFILE)) {
				ret = avformat_write_header(context, NULL);
				if (ret < 0) {
					LOG->error(L"failed to write header: {}", av2_error_string(ret));
					avio_closep(&context->pb);
				}
			}
		}
		LOG_EXIT;
	}

	void SendVideoFrame() {
		if (process_packet) {
			vstream->ProcessFrame(*process_packet);
		}
	}

	void SendAudioFrame() {
		if (process_packet) {
			astream->ProcessFrame(*process_packet);
		}
		LOG_EXIT;
	}

	~Format() {
		LOG_ENTER;
		if (process_packet) {
			if (vstream) vstream->Flush(*process_packet);
			if (astream) astream->Flush(*process_packet);
		}
		if (context && context->pb) {
			int ret = av_write_trailer(context);
			if (ret < 0) {
				LOG->error(L"failed to write trailer: {}", av2_error_string(ret));
			}
		}
		vstream = nullptr;
		astream = nullptr;
		if (context) {
			avio_closep(&context->pb);
			avformat_free_context(context);
		}
		LOG_EXIT;
	}
};

int main()
{
	/* load settings without logger, to get log_level and log_flush_on */
	logger = spdlog::stdout_color_mt(SCRIPT_NAME);
	settings.reset(new Settings);
	av_log_set_level(spdlog_av_level(logger->level()));
	av_log_set_callback(av_log_callback);
	LOG_ENTER;
	std::wostringstream os;
	settings->generate(os);
	LOG->trace(L"settings before interpolation:\n{}", os.str());
	settings->interpolate();
	os.str(L"");
	settings->generate(os);
	LOG->trace(L"settings after interpolation:\n{}", os.str());
	auto format = std::unique_ptr<Format>(new Format{});
	const auto total_time = 5.0;
	while (format->astream->frame->pts < total_time * format->astream->context->sample_rate) {
		format->astream->NextFrame();
		format->SendAudioFrame();
		while (format->astream->frame->pts * av_q2d(format->astream->context->time_base) > format->vstream->frame->pts * av_q2d(format->vstream->context->time_base)) {
			format->vstream->NextFrame();
			format->SendVideoFrame();
		}
	}
	format = nullptr;
	LOG_EXIT;
	std::cin.get();
}
