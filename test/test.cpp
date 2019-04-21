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
#include "stream.h"

#pragma comment(lib, "common.lib")

std::shared_ptr<spdlog::logger> logger = nullptr;
std::unique_ptr<Settings> settings = nullptr;

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

auto MakeVideoFrameData(size_t width, size_t height, AVPixelFormat pix_fmt, double t) {
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

class VideoStream : public Stream {
public:
	const AVPixelFormat pix_fmt;

	VideoStream(
		AVFormatContext& format_context, AVCodecID codec_id,
		int width, int height,
		const AVRational& frame_rate,
		AVPixelFormat pix_fmt)
		: Stream{ format_context, codec_id }, pix_fmt{ pix_fmt }
	{
		LOG_ENTER;
		if (context) {
			if (context->codec && context->codec->type != AVMEDIA_TYPE_VIDEO) {
				LOG->warn("selected video codec {} does not support video", context->codec->name);
			}
			context->width = width;
			context->height = height;
			context->time_base = av_inv_q(frame_rate);
			int loss = 0;
			if (context->codec && context->codec->pix_fmts) {
				context->pix_fmt = avcodec_find_best_pix_fmt_of_list(context->codec->pix_fmts, pix_fmt, 0, &loss);
			}
			else {
				context->pix_fmt = pix_fmt;
			}
			if (context->pix_fmt != pix_fmt) {
				LOG->info(
					"pixel format {} not supported by codec so using {} instead",
					av_get_pix_fmt_name(pix_fmt), av_get_pix_fmt_name(context->pix_fmt));
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
				LOG->error("failed to open video codec: {}", AVErrorString(ret));
			}
			if (stream) {
				stream->time_base = context->time_base;
				avcodec_parameters_from_context(stream->codecpar, context);
			}
			if (frame) {
				frame->width = width;
				frame->height = height;
				frame->format = context->pix_fmt;
				int ret = av_frame_get_buffer(frame, 32);
				if (ret < 0) {
					LOG->error("failed to allocate frame buffer");
				}
			}
		}
		LOG_EXIT;
	}

	void MakeFrame(uint8_t* ptr) {
		LOG_ENTER;
		struct SwsContext* sws = sws_getContext(
			frame->width, frame->height, pix_fmt,
			frame->width, frame->height, context->pix_fmt,
			SWS_BICUBIC, nullptr, nullptr, nullptr);
		if (!sws) {
			LOG->error("failed to initialize pixel conversion context");
		}
		else {
			uint8_t* data[4];
			int linesize[4];
			av_image_fill_linesizes(linesize, pix_fmt, frame->width);
			av_image_fill_pointers(data, pix_fmt, frame->height, ptr, linesize);
			sws_scale(
				sws,
				data, linesize, 0, frame->height,
				frame->data, frame->linesize);
			sws_freeContext(sws);
		}
		LOG_EXIT;
	};
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
				LOG->info(
					"sample format {} not supported by codec so using {} instead",
					av_get_sample_fmt_name(sample_fmt), av_get_sample_fmt_name(context->sample_fmt));
			}
			context->sample_rate = sample_rate;
			context->channel_layout = channel_layout;
			context->channels = av_get_channel_layout_nb_channels(channel_layout);
			context->time_base = AVRational{ 1, sample_rate };
			int ret = 0;
			ret = avcodec_open2(context, NULL, NULL);
			if (ret < 0) {
				LOG->error("failed to open audio codec: {}", AVErrorString(ret));
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

	Format(
		const std::string& filename,
		AVCodecID vcodec, int width, int height, const AVRational& frame_rate, AVPixelFormat pix_fmt,
		AVCodecID acodec, AVSampleFormat sample_fmt, int sample_rate, uint64_t channel_layout)
		: context{ nullptr }, vstream{ nullptr }, astream{ nullptr }
	{
		LOG_ENTER;
		int ret = 0;
		ret = avformat_alloc_output_context2(&context, NULL, NULL, filename.c_str());
		if (ret < 0) {
			LOG->error("failed to allocate output context for '{}': {}", filename, AVErrorString(ret));
		}
		if (context) {
			vstream.reset(new VideoStream(*context, vcodec, width, height, frame_rate, pix_fmt));
			astream.reset(new AudioStream(*context, acodec, sample_fmt, sample_rate, channel_layout));
			av_dump_format(context, 0, filename.c_str(), 1);
			ret = avio_open(&context->pb, filename.c_str(), AVIO_FLAG_WRITE);
			if (ret < 0) {
				LOG->error("failed to open '{}' for writing: {}", filename, AVErrorString(ret));
			}
		}
		if (context && context->pb) {
			if (!(context->oformat->flags & AVFMT_NOFILE)) {
				ret = avformat_write_header(context, NULL);
				if (ret < 0) {
					LOG->error("failed to write header: {}", AVErrorString(ret));
					avio_closep(&context->pb);
				}
			}
		}
		LOG_EXIT;
	}

	~Format() {
		LOG_ENTER;
		if (context) {
			if (vstream) vstream->Flush(context);
			if (astream) astream->Flush(context);
		}
		if (context && context->pb) {
			int ret = av_write_trailer(context);
			if (ret < 0) {
				LOG->error("failed to write trailer: {}", AVErrorString(ret));
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
	AVLogSetCallback();
	logger = spdlog::stdout_color_mt(SCRIPT_NAME);
	settings.reset(new Settings);
	LOG_ENTER;
	std::wostringstream os;
	settings->generate(os);
	LOG->trace(L"settings before interpolation:\n{}", os.str());
	settings->interpolate();
	os.str(L"");
	settings->generate(os);
	LOG->trace(L"settings after interpolation:\n{}", os.str());
	std::wstring base;
	auto exportsec = settings->GetSec(L"export");
	settings->GetVar(exportsec, L"base", base);
	std::wstring filename{ base + L".mkv" };
	std::string ufilename{ wstring_to_utf8(filename) };
	LOG->info("export started");
	auto pix_fmt = AV_PIX_FMT_NV12;
	auto width = 426;
	auto height = 240;
	auto format = std::unique_ptr<Format>(new Format(
		ufilename,
		AV_CODEC_ID_FFV1, width, height, AVRational{ 30000, 1001 }, pix_fmt,
		AV_CODEC_ID_FLAC, AV_SAMPLE_FMT_S16, 44100, AV_CH_LAYOUT_STEREO));
	while (format->astream->Time() < 5.0) {
		auto & astream = format->astream;
		astream->NextFrame();
		astream->SendFrame(format->context);
		while (format->astream->Time() > format->vstream->Time()) {
			auto & vstream = format->vstream;
			auto t = vstream->frame->pts * av_q2d(vstream->context->time_base);
			auto data = MakeVideoFrameData(width, height, pix_fmt, t);
			vstream->MakeFrame(data.get());
			vstream->SendFrame(format->context);
			vstream->frame->pts += 1;
		}
	}
	format = nullptr;
	LOG->info("export finished");
	LOG_EXIT;
	std::cin.get();
}
