#include <iostream>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/pixdesc.h>
}

#include "settings.h"

#pragma comment(lib, "common.lib")

std::shared_ptr<spdlog::logger> logger = nullptr;
std::unique_ptr<Settings> settings = nullptr;
std::mutex av_log_mutex;

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

extern "C" {
	void av_log_callback(void* avcl, int level, const char* fmt, va_list vl);
}

void av_log_callback(void* avcl, int level, const char* fmt, va_list vl)
{
	// each thread should have its own character buffer
	thread_local char line[256] = { 0 };
	thread_local int pos = 0;

	std::lock_guard<std::mutex> lock(av_log_mutex);
	int print_prefix = 1;
	int remain = sizeof(line) - pos;
	int ret = av_log_format_line2(avcl, level, fmt, vl, line + pos, remain, &print_prefix);
	if (ret >= 0) {
		pos += (ret <= remain) ? ret : remain;
	}
	else {
		// log at the specified level rather than error level to avoid spamming the log
		LOG->log(av_spdlog_level(level), "failed to format av_log message '{}'", fmt);
	}
	// only write log message on newline
	if ((pos > 0) && *(line + pos - 1) == '\n') {
		*(line + pos - 1) = '\0';
		LOG->log(av_spdlog_level(level), line);
		pos = 0;
	}
}

std::wstring av_error_string(int errnum) {
	char buffer[256];
	av_make_error_string(buffer, sizeof(buffer), errnum);
	return wstring_from_utf8(std::string(buffer));
}


class Stream {
public:
	AVStream* stream;
	AVCodecContext* context;

	Stream(AVFormatContext* format_context, std::wstring codec_name, AVMediaType media_type) :
		stream{ nullptr }, context{ nullptr }
	{
		LOG_ENTER;
		AVCodec* codec = nullptr;
		if (!codec_name.empty()) {
			codec = avcodec_find_encoder_by_name(wstring_to_utf8(codec_name).c_str());
			if (!codec) {
				LOG->error(L"failed to find encoder '{}'", codec_name);
			}
		}
		if (!codec) {
			switch (media_type) {
			case AVMEDIA_TYPE_VIDEO:
				codec = avcodec_find_encoder(format_context->oformat->video_codec);
				break;
			case AVMEDIA_TYPE_AUDIO:
				codec = avcodec_find_encoder(format_context->oformat->audio_codec);
				break;
			}
			if (codec) {
				LOG->info("using default encoder '{}'", avcodec_get_name(codec->id));
			}
		}
		stream = avformat_new_stream(format_context, codec);
		if (!stream) {
			LOG->error(L"failed to allocate '{}' codec stream", codec_name);
		}
		context = avcodec_alloc_context3(codec);
		if (!context) {
			LOG->error(L"failed to allocate '{}' codec context", codec_name);
		}
		LOG_EXIT;
	}

	~Stream() {
		if (context) {
			avcodec_free_context(&context);
		}
		stream = nullptr;
	}
};

class VideoStream : public Stream {
public:
	VideoStream(
		AVFormatContext* format_context, std::wstring codec_name,
		int width, int height,
		int framerate_numerator, int framerate_denominator,
		AVPixelFormat pix_fmt)
		: Stream{ format_context, codec_name, AVMEDIA_TYPE_VIDEO }
	{
		LOG_ENTER;
		if (context) {
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
				LOG->info("pixel format '{}' not supported by codec", av_get_pix_fmt_name(pix_fmt));
				LOG->info("using pixel format '{}' instead", av_get_pix_fmt_name(context->pix_fmt));
			}
			if (loss) {
				LOG->warn(
					"pixel format conversion from '{}' to '{}' is lossy",
					av_get_pix_fmt_name(pix_fmt),
					av_get_pix_fmt_name(context->pix_fmt));
			}
			int ret = 0;
			ret = avcodec_open2(context, NULL, NULL);
			if (ret < 0) {
				LOG->error(L"failed to open video codec: {}", av_error_string(ret));
			}
			if (stream) {
				stream->time_base = context->time_base;
				avcodec_parameters_from_context(stream->codecpar, context);
			}
		}
		LOG_EXIT;
	}

};

class AudioStream : public Stream {
public:
	AudioStream(
		AVFormatContext* format_context, std::wstring codec_name,
		AVSampleFormat sample_fmt, int sample_rate, uint64_t channel_layout)
		: Stream{ format_context, codec_name, AVMEDIA_TYPE_AUDIO }
	{
		LOG_ENTER;
		if (context) {
			context->sample_fmt = sample_fmt;
			if (context->codec && context->codec->sample_fmts) {
				const AVSampleFormat* sample_fmts = context->codec->sample_fmts;
				context->sample_fmt = sample_fmts[0];
				for (int i = 0; sample_fmts[i]; i++) {
					if (sample_fmts[i] == sample_fmt) {
						context->sample_fmt = sample_fmts[i];
						break;
					}
				}
			}
			if (context->sample_fmt != sample_fmt) {
				LOG->info("sample format '{}' not supported by codec", av_get_sample_fmt_name(sample_fmt));
				LOG->info("using sample format '{}' instead", av_get_sample_fmt_name(context->sample_fmt));
			}
			context->sample_rate = sample_rate;
			context->channel_layout = channel_layout;
			context->channels = av_get_channel_layout_nb_channels(channel_layout);
			int ret = 0;
			ret = avcodec_open2(context, NULL, NULL);
			if (ret < 0) {
				LOG->error(L"failed to open audio codec: {}", av_error_string(ret));
			}
			if (stream) {
				stream->time_base = AVRational{ 1, sample_rate };
				avcodec_parameters_from_context(stream->codecpar, context);
			}
		}
		LOG_EXIT;
	}
};

class Format {
public:
	AVFormatContext* context;
	std::unique_ptr<VideoStream> vstream;
	std::unique_ptr<AudioStream> astream;

	Format() : context{ nullptr }, vstream{ nullptr }, astream{ nullptr }
	{
		LOG_ENTER;
		std::wstring preset_name;
		std::wstring container;
		std::wstring acodec;
		std::wstring vcodec;
		std::wstring base;
		auto topsec = settings->GetSec(L"");
		settings->GetVar(topsec, L"preset", preset_name);
		auto presetsec = settings->GetSec(preset_name);
		settings->GetVar(presetsec, L"container", container);
		settings->GetVar(presetsec, L"audiocodec", acodec);
		settings->GetVar(presetsec, L"videocodec", vcodec);
		auto exportsec = settings->GetSec(L"export");
		settings->GetVar(exportsec, L"base", base);
		std::wstring filename{ base + L"." + container };
		std::string ufilename{ wstring_to_utf8(filename) };
		int ret = 0;
		ret = avformat_alloc_output_context2(&context, NULL, NULL, ufilename.c_str());
		if (ret < 0) {
			LOG->error(L"failed to allocate output context for '{}': {}", filename, av_error_string(ret));
		}
		if (context) {
			vstream.reset(new VideoStream(context, vcodec, 1920, 1080, 60000, 1001, AV_PIX_FMT_NV12));
			astream.reset(new AudioStream(context, acodec, AV_SAMPLE_FMT_S16, 44100, AV_CH_LAYOUT_STEREO));
			av_dump_format(context, 0, ufilename.c_str(), 1);
			ret = avio_open(&context->pb, ufilename.c_str(), AVIO_FLAG_WRITE);
			if (ret < 0) {
				LOG->error(L"failed to open '{}' for writing: {}", filename, av_error_string(ret));
			}
		}
		if (context && context->pb) {
			if (!(context->oformat->flags & AVFMT_NOFILE)) {
				ret = avformat_write_header(context, NULL);
				if (ret < 0) {
					LOG->error(L"failed to write header: {}", av_error_string(ret));
					avio_closep(&context->pb);
				}
			}
		}
		LOG_EXIT;
	}

	~Format() {
		LOG_ENTER;
		if (context && context->pb) {
			int ret = av_write_trailer(context);
			if (ret < 0) {
				LOG->error(L"failed to write trailer: {}", av_error_string(ret));
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
	format = nullptr;
	LOG_EXIT;
	std::cin.get();
}
