#include <iostream>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

#include "settings.h"

#pragma comment(lib, "common.lib")

std::shared_ptr<spdlog::logger> logger = nullptr;
std::unique_ptr<Settings> settings = nullptr;

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
	int print_prefix = 1;
	char line[256];
	av_log_format_line(avcl, level, fmt, vl, line, sizeof(line), &print_prefix);
	// remove trailing newline
	size_t n{ strlen(line) };
	if (n > 0 && line[n - 1] == '\n') line[n - 1] = '\0';
	LOG->log(av_spdlog_level(level), line);
}

class AVContext {
public:
	AVFormatContext* oc;
	AVCodec* audio_codec;
	AVCodec* video_codec;

	AVContext() : oc{ nullptr }, audio_codec{ nullptr }, video_codec{ nullptr } {
		std::wstring preset_name;
		std::wstring container;
		std::wstring acodec;
		std::wstring vcodec;
		std::wstring filter;
		std::wstring base;
		auto topsec = settings->GetSec(L"");
		settings->GetVar(topsec, L"preset", preset_name);
		auto presetsec = settings->GetSec(preset_name);
		settings->GetVar(presetsec, L"container", container);
		settings->GetVar(presetsec, L"audiocodec", acodec);
		settings->GetVar(presetsec, L"videocodec", vcodec);
		settings->GetVar(presetsec, L"filter", filter);
		auto exportsec = settings->GetSec(L"export");
		settings->GetVar(exportsec, L"base", base);
		std::wstring filename = base + L"." + container;
		int ret = 0;
		ret = avformat_alloc_output_context2(&oc, NULL, NULL, wstring_to_utf8(filename).c_str());
		if ((ret < 0) || !oc) {
			logger->error(L"failed to allocate format context for container '{}'", container);
		}
		audio_codec = avcodec_find_encoder_by_name(wstring_to_utf8(acodec).c_str());
		if (!audio_codec) {
			logger->error(L"failed to find '{}' encoder", acodec);
			audio_codec = nullptr;
		}
		else {
			if (audio_codec->type != AVMEDIA_TYPE_AUDIO) {
				logger->error(L"'{}' cannot encode audio", acodec);
				audio_codec = nullptr;
			}
		}
		video_codec = avcodec_find_encoder_by_name(wstring_to_utf8(vcodec).c_str());
		if (!video_codec) {
			logger->error(L"failed to find '{}' encoder", vcodec);
		}
		else {
			if (video_codec->type != AVMEDIA_TYPE_VIDEO) {
				logger->error(L"'{}' cannot encode video", vcodec);
				video_codec = nullptr;
			}
		}
	}

	~AVContext() {
		avformat_free_context(oc);
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
	LOG->debug(L"settings before interpolation:\n{}", os.str());
	settings->interpolate();
	os.str(L"");
	settings->generate(os);
	LOG->debug(L"settings after interpolation:\n{}", os.str());
	AVContext context{};
	LOG_EXIT;
	std::cin.get();
}
