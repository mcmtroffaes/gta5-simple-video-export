#include <codecvt>
#include <iostream>
#include <memory>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavutil/pixdesc.h>
#include <libavutil/timestamp.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}

#include "format.h"
#include "settings.h"

#pragma comment(lib, "common.lib")

std::shared_ptr<spdlog::logger> logger = nullptr;
std::unique_ptr<Settings> settings = nullptr;

#define DATAY(S, X, Y, T) (uint8_t)((S) * (X) + (S) * (Y) + (T) * 30)
#define DATAU(S, X, Y, T) (uint8_t)(128 + (S) * (Y) + (T) * 20)
#define DATAV(S, X, Y, T) (uint8_t)(64 + (S) * (X) + (T) * 50)

auto MakeVideoData(size_t width, size_t height, AVPixelFormat pix_fmt, double t) {
	LOG_ENTER;
	std::unique_ptr<uint8_t[]> data{ nullptr };
	size_t i{ 0 };
	double s{ 416.0 / width };
	switch (pix_fmt) {
	case AV_PIX_FMT_NV12:
		data = std::make_unique<uint8_t[]>(width * height + 2 * ((width / 2) * (height / 2)));
		for (int y = 0; y < height; y++)
			for (int x = 0; x < width; x++)
				data[i++] = DATAY(s, x, y, t);
		for (int y = 0; y < height / 2; y++) {
			for (int x = 0; x < width / 2; x++) {
				data[i++] = DATAU(s, x, y, t);
				data[i++] = DATAV(s, x, y, t);
			}
		}
		break;
	case AV_PIX_FMT_YUV420P:
		data = std::make_unique<uint8_t[]>(width * height + 2 * ((width / 2) * (height / 2)));
		for (int y = 0; y < height; y++)
			for (int x = 0; x < width; x++)
				data[i++] = DATAY(s, x, y, t);
		for (int y = 0; y < height / 2; y++)
			for (int x = 0; x < width / 2; x++)
				data[i++] = DATAU(s, x, y, t);
		for (int y = 0; y < height / 2; y++)
			for (int x = 0; x < width / 2; x++)
				data[i++] = DATAV(s, x, y, t);
		break;
	case AV_PIX_FMT_YUV444P:
		data = std::make_unique<uint8_t[]>(3 * width * height);
		for (int y = 0; y < height; y++)
			for (int x = 0; x < width; x++)
				data[i++] = DATAY(s, x, y, t);
		for (int y = 0; y < height; y++)
			for (int x = 0; x < width; x++)
				data[i++] = DATAU(s, 0.5 * x, 0.5 * y, t);
		for (int y = 0; y < height; y++)
			for (int x = 0; x < width; x++)
				data[i++] = DATAV(s, 0.5 * x, 0.5 * y, t);
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
	auto* q_u8 = data.get();
	auto* q_s16 = reinterpret_cast<int16_t*>(data.get());
	auto* q_s32 = reinterpret_cast<int32_t*>(data.get());
	auto* q_flt = reinterpret_cast<float*>(data.get());
	auto* q_dbl = reinterpret_cast<double*>(data.get());
	uint8_t a_u8 = 85; // 255 / 3;
	int16_t a_s16 = 21845; // 65535 / 3;
	int32_t a_s32 = 1431655765; // 4294967295 / 3;
	switch (sample_fmt) {
	case (AV_SAMPLE_FMT_U8):
		for (int j = 0; j < nb_samples; j++)
			for (int k = 0; k < channels; k++)
				*q_u8++ = 128 + (int8_t)(a_u8 * rawdata[j]);
		break;
	case (AV_SAMPLE_FMT_U8P):
		for (int k = 0; k < channels; k++)
			for (int j = 0; j < nb_samples; j++)
				*q_u8++ = 128 + (int8_t)(a_u8 * rawdata[j]);
		break;
	case (AV_SAMPLE_FMT_S16):
		for (int j = 0; j < nb_samples; j++)
			for (int k = 0; k < channels; k++)
				*q_s16++ = (int16_t)(a_s16 * rawdata[j]);
		break;
	case (AV_SAMPLE_FMT_S16P):
		for (int k = 0; k < channels; k++)
			for (int j = 0; j < nb_samples; j++)
				*q_s16++ = (int16_t)(a_s16 * rawdata[j]);
		break;
	case (AV_SAMPLE_FMT_S32):
		for (int j = 0; j < nb_samples; j++)
			for (int k = 0; k < channels; k++)
				*q_s32++ = (int32_t)(a_s32 * rawdata[j]);
		break;
	case (AV_SAMPLE_FMT_S32P):
		for (int k = 0; k < channels; k++)
			for (int j = 0; j < nb_samples; j++)
				*q_s32++ = (int32_t)(a_s32 * rawdata[j]);
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
	case (AV_SAMPLE_FMT_DBL):
		for (int j = 0; j < nb_samples; j++)
			for (int k = 0; k < channels; k++)
				*q_dbl++ = (double)(0.3 * rawdata[j]);
		break;
	case (AV_SAMPLE_FMT_DBLP):
		for (int k = 0; k < channels; k++)
			for (int j = 0; j < nb_samples; j++)
				*q_dbl++ = (double)(0.3 * rawdata[j]);
		break;
	default:
		LOG->error("unsupported sample format");
		break;
	}
	LOG_EXIT;
	return data;
}

void Test(
	const std::filesystem::path& filename,
	AVCodecID vcodec_id, AVRational frame_rate, AVPixelFormat pix_fmt,
	AVCodecID acodec_id, AVSampleFormat sample_fmt, int sample_rate, uint64_t channel_layout)
{
	LOG->info("export started");
	auto width = 416;
	auto height = 234;
	std::unique_ptr<Format> format{ nullptr };
	format = std::make_unique<Format>(
		filename,
		vcodec_id, width, height, frame_rate, pix_fmt,
		acodec_id, sample_fmt, sample_rate, channel_layout);
	const auto atb = AVRational{ 1, sample_rate };
	const auto vtb = av_inv_q(frame_rate);
	int apts = 0;
	int vpts = 0;
	while (apts * av_q2d(atb) < 5.0) {
		while (av_compare_ts(apts, atb, vpts, vtb) <= 0) {
			const auto nb_samples = 1000;
			auto adata = MakeAudioData(
				sample_fmt, sample_rate, channel_layout,
				nb_samples, apts);
			auto aframe = CreateAudioFrame(sample_fmt, sample_rate, channel_layout, nb_samples, adata.get());
			format->astream->Transcode(aframe);
			apts += nb_samples;
		}
		while (av_compare_ts(apts, atb, vpts, vtb) >= 0) {
			auto vdata = MakeVideoData(
				width, height, pix_fmt,
				vpts * av_q2d(vtb));
			auto vframe = CreateVideoFrame(width, height, pix_fmt, vdata.get());
			format->vstream->Transcode(vframe);
			vpts++;
		}
	}
	format->Flush();
	format = nullptr;
	LOG->info("export finished");
	LOG->info("exported {} video frames and {} audio frames", vpts, apts);
	LOG_EXIT;
}

int main()
{
	logger = spdlog::stdout_color_mt(SCRIPT_NAME);
	logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%t] [%l] %v");
	AVLogSetCallback();
	settings = std::make_unique<Settings>();
	LOG_ENTER;
	auto frame_rate_numerator{ 30000 };
	auto frame_rate_denominator{ 1001 };
	std::string pix_fmt_name{ "yuv420p" };
	std::string sample_fmt_name{ "s16" };
	auto sample_rate{ 44100 };
	auto nb_channels{ 2 };
	auto testsec = settings->GetSec("test");
	settings->GetVar(testsec, "frame_rate_numerator", frame_rate_numerator);
	settings->GetVar(testsec, "frame_rate_denominator", frame_rate_denominator);
	settings->GetVar(testsec, "pix_fmt", pix_fmt_name);
	settings->GetVar(testsec, "sample_fmt", sample_fmt_name);
	settings->GetVar(testsec, "sample_rate", sample_rate);
	settings->GetVar(testsec, "nb_channels", nb_channels);
	auto pix_fmt = av_get_pix_fmt(pix_fmt_name.c_str());
	auto sample_fmt = av_get_sample_fmt(sample_fmt_name.c_str());
	if (pix_fmt == AV_PIX_FMT_NONE) {
		LOG->error("test pixel format {} not found, falling back on yuv420p", pix_fmt_name);
		pix_fmt = AV_PIX_FMT_YUV420P;
	}
	if (sample_fmt == AV_SAMPLE_FMT_NONE) {
		LOG->error("test sample format {} not found, falling back on s16", sample_fmt_name);
		sample_fmt = AV_SAMPLE_FMT_S16;
	}
	try {
		Test(
			settings->export_filename,
			settings->video_codec_id, AVRational{ frame_rate_numerator, frame_rate_denominator }, pix_fmt,
			settings->audio_codec_id, sample_fmt, sample_rate, av_get_default_channel_layout(nb_channels));
	}
	catch (std::exception& e) {
	}
	std::cout << "Press enter...";
	std::cin.get();
	LOG_EXIT;
}
