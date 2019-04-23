#include <codecvt>
#include <iostream>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavutil/pixdesc.h>
#include <libavutil/timestamp.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
}

#include "settings.h"
#include "format.h"

#pragma comment(lib, "common.lib")

std::shared_ptr<spdlog::logger> logger = nullptr;
std::unique_ptr<Settings> settings = nullptr;

auto MakeVideoData(size_t width, size_t height, AVPixelFormat pix_fmt, double t) {
	LOG_ENTER;
	std::unique_ptr<uint8_t[]> data{ nullptr };
	size_t i{ 0 };
	switch (pix_fmt) {
	case AV_PIX_FMT_NV12:
		data = std::make_unique<uint8_t[]>(width * height + 2 * ((width / 2) * (height / 2)));
		for (int y = 0; y < height; y++)
			for (int x = 0; x < width; x++)
				data[i++] = (uint8_t)(x + y + t * 30);
		for (int y = 0; y < height / 2; y++) {
			for (int x = 0; x < width / 2; x++) {
				data[i++] = (uint8_t)(128 + y + t * 20);
				data[i++] = (uint8_t)(64 + x + t * 50);
			}
		}
		break;
	case AV_PIX_FMT_YUV420P:
		data = std::make_unique<uint8_t[]>(width * height + 2 * ((width / 2) * (height / 2)));
		for (int y = 0; y < height; y++)
			for (int x = 0; x < width; x++)
				data[i++] = (uint8_t)(x + y + t * 30);
		for (int y = 0; y < height / 2; y++)
			for (int x = 0; x < width / 2; x++)
				data[i++] = (uint8_t)(128 + y + t * 20);
		for (int y = 0; y < height / 2; y++)
			for (int x = 0; x < width / 2; x++)
				data[i++] = (uint8_t)(64 + x + t * 50);
		break;
	case AV_PIX_FMT_YUV444P:
		data = std::make_unique<uint8_t[]>(3 * width * height);
		for (int y = 0; y < height; y++)
			for (int x = 0; x < width; x++)
				data[i++] = (uint8_t)(x + y + t * 30);
		for (int y = 0; y < height; y++)
			for (int x = 0; x < width; x++)
				data[i++] = (uint8_t)(128 + 0.5 * y + t * 20);
		for (int y = 0; y < height; y++)
			for (int x = 0; x < width; x++)
				data[i++] = (uint8_t)(64 + 0.5 * x + t * 50);
		break;
	default:
		LOG->error("unsupported pixel format");
	}
	LOG_EXIT;
	return data;
};

auto MakeAudioData(AVSampleFormat sample_fmt, int sample_rate, uint64_t channel_layout, int nb_samples, uint64_t pts) {
	LOG_ENTER;
	const auto freq1 = 220.0;
	const auto freq2 = 220.0 * 5.0 / 4.0; // perfect third
	const auto freq3 = 1.0;
	const auto delta = 0.5;
	const auto channels = av_get_channel_layout_nb_channels(channel_layout);
	auto data{ std::make_unique<uint8_t[]>(av_samples_get_buffer_size(NULL, channels, nb_samples, sample_fmt, 1)) };
	auto rawdata{ std::make_unique<double[]>(nb_samples) };
	for (int j = 0; j < nb_samples; j++) {
		auto two_pi_time = (2.0 * M_PI * (pts + j)) / sample_rate;
		rawdata[j] = sin(freq1 * two_pi_time + delta * sin(freq2 * two_pi_time) * sin(freq3 * two_pi_time));
	}
	int16_t* q_s16 = (int16_t*)data.get();
	float* q_flt = (float*)data.get();
	switch (sample_fmt) {
	case (AV_SAMPLE_FMT_S16):
		for (int j = 0; j < nb_samples; j++)
			for (int k = 0; k < channels; k++)
				*q_s16++ = (int)(10000 * rawdata[j]);
		break;
	case (AV_SAMPLE_FMT_S16P):
		for (int k = 0; k < channels; k++)
			for (int j = 0; j < nb_samples; j++)
				*q_s16++ = (int)(10000 * rawdata[j]);
		break;
	case (AV_SAMPLE_FMT_FLT):
		for (int j = 0; j < nb_samples; j++)
			for (int k = 0; k < channels; k++)
				*q_flt++ = (float)(0.3 * rawdata[j]);
		break;
	case (AV_SAMPLE_FMT_FLTP):
		for (int k = 0; k < channels; k++)
			for (int j = 0; j < nb_samples; j++)
				*q_flt++ = (float)(0.3 * rawdata[j]);
		break;
	default:
		LOG->error("unsupported sample format");
		break;
	}
	LOG_EXIT;
	return data;
}

int main()
{
	AVLogSetCallback();
	logger = spdlog::stdout_color_mt(SCRIPT_NAME);
	settings.reset(new Settings);
	LOG_ENTER;
	std::ostringstream os;
	settings->generate(os);
	LOG->trace("settings before interpolation:\n{}", os.str());
	settings->interpolate();
	os.str("");
	settings->generate(os);
	LOG->trace("settings after interpolation:\n{}", os.str());
	std::string base{ "simple-video-export-test-video" };
	auto exportsec = settings->GetSec("export");
	settings->GetVar(exportsec, "base", base);
	std::string filename{ base + ".mkv" };
	LOG->info("export started");
	auto pix_fmt = AV_PIX_FMT_NV12;
	auto width = 416;
	auto height = 234;
	auto sample_fmt = AV_SAMPLE_FMT_S16;
	auto sample_rate = 10000;
	std::unique_ptr<Format> format{ nullptr };
	format = std::unique_ptr<Format>(new Format(
		filename,
		AV_CODEC_ID_FFV1, width, height, AVRational{ 30000, 1001 }, pix_fmt,
		AV_CODEC_ID_AAC, sample_fmt, sample_rate, AV_CH_LAYOUT_STEREO));
	int apts = 0;
	while (format->astream->Time() < 5.0) {
		while (format->vstream->Time() >= format->astream->Time()) {
			const auto nb_samples = 1000;
			auto adata = MakeAudioData(
				sample_fmt, sample_rate, AV_CH_LAYOUT_STEREO,
				nb_samples, apts);
			format->astream->Transcode(adata.get(), nb_samples);
			apts += nb_samples;
		}
		while (format->astream->Time() >= format->vstream->Time()) {
			auto vdata = MakeVideoData(
				width, height, pix_fmt,
				format->vstream->Time());
			format->vstream->Transcode(vdata.get());
		}
	}
	format->Flush();
	format = nullptr;
	LOG->info("export finished");
	LOG_EXIT;
}
