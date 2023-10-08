/*
Hooks into media foundation's sink writer for intercepting video/audio data.

The main entry point is CreateSinkWriterFromURL. This reloads the ini file (so
we always have the latest settings; coincidently this also updates the
timestamp), checks if the mod is enabled, and sets up all the required
SinkWriter hooks.

Under normal circumstances, the game will then call SinkWriterSetInputMediaType
twice, once for the audio, and once for the video. At this point, we intercept
the audio and video information (resolution, format, ...) and store those the
audio_info and video_info variables.

After that, the game will call SinkWriterBeginWriting. There, we create the
format variable which provides the main interface for encoding the audio and
video data.

Next, the game will repeatedly call SinkWriterWriteSample. We intercept the
raw data and transcode it ourselves. Note that SinkWriterWriteSample is
called from different threads for audio and for video, so we must ensure
that all transcode calls are thread safe. This is the purpose of
format_mutex. For safety, we lock this mutex whenever we access format.

At the end of the export process, the game calls SinkWriterFinalize if the
export finished normally, or Flush if the export is cancelled. There we
flush the encoder, clear the format (this will finalize the file), and unhook
all the SinkWriter hooks.
*/

#include "sinkwriter.h"
#include "logger.h"
#include "settings.h"
#include "hook.h"
#include "info.h"
#include "format.h"

#include <mutex>

#include <winrt/base.h> // com_ptr

#include <mfapi.h>
#include <mfplay.h>
#include <mfreadwrite.h>
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")

extern "C" {
#include <libavutil/pixdesc.h>
}

typedef VFunc<4, decltype(&SinkWriterSetInputMediaType)> VSinkWriterSetInputMediaType;
typedef VFunc<5, decltype(&SinkWriterBeginWriting)> VSinkWriterBeginWriting;
typedef VFunc<6, decltype(&SinkWriterWriteSample)> VSinkWriterWriteSample;
typedef VFunc<10, decltype(&SinkWriterFlush)> VSinkWriterFlush;
typedef VFunc<11, decltype(&SinkWriterFinalize)> VSinkWriterFinalize;
std::unique_ptr<IatHook<decltype(&CreateSinkWriterFromURL)>> create_sinkwriter_hook = nullptr;
std::unique_ptr<VTableSwapHook> sinkwriter_hook = nullptr;
std::unique_ptr<AudioInfo> audio_info = nullptr;
std::unique_ptr<VideoInfo> video_info = nullptr;
std::unique_ptr<Format> format = nullptr;
std::mutex format_mutex;

void UnhookVFuncDetours()
{
	LOG_ENTER;
	sinkwriter_hook = nullptr;
	audio_info = nullptr;
	video_info = nullptr;
	{
		std::lock_guard<std::mutex> lock(format_mutex);
		format = nullptr;
	}
	LOG_EXIT;
}

void Unhook()
{
	LOG_ENTER;
	UnhookVFuncDetours();
	create_sinkwriter_hook = nullptr;
	LOG_EXIT;
}

STDAPI SinkWriterSetInputMediaType(
	IMFSinkWriter *pThis,
	DWORD         dwStreamIndex,
	IMFMediaType  *pInputMediaType,
	IMFAttributes *pEncodingParameters)
{
	LOG_ENTER;
	auto hr = E_FAIL;
	try {
		if (!sinkwriter_hook)
			throw std::runtime_error("IMFSinkWriter hook not set up");
		hr = sinkwriter_hook->orig_func<VSinkWriterSetInputMediaType>(pThis, dwStreamIndex, pInputMediaType, pEncodingParameters);
		THROW_FAILED(hr);
		GUID major_type = { 0 };
		if (!pInputMediaType) {
			throw std::runtime_error("input media type pointer is null");
		}
		THROW_FAILED(pInputMediaType->GetMajorType(&major_type));
		if (major_type == MFMediaType_Audio) {
			audio_info = std::make_unique<AudioInfo>(dwStreamIndex, *pInputMediaType);
		}
		else if (major_type == MFMediaType_Video) {
			video_info = std::make_unique<VideoInfo>(dwStreamIndex, *pInputMediaType);
		}
		else {
			LOG->debug("unknown stream at index {}", dwStreamIndex);
		}
	}
	LOG_CATCH;
	LOG_EXIT;
	return hr;
}

STDAPI SinkWriterBeginWriting(
	IMFSinkWriter *pThis)
{
	LOG_ENTER;
	auto hr = E_FAIL;
	try {
		if (!sinkwriter_hook)
			throw std::runtime_error("IMFSinkWriter hook not set up");
		hr = sinkwriter_hook->orig_func<VSinkWriterBeginWriting>(pThis);
		THROW_FAILED(hr);
	}
	LOG_CATCH;
	try {
		if (settings && audio_info && video_info) {
			std::lock_guard<std::mutex> lock(format_mutex);
			format = std::make_unique<Format>(
				settings->export_filename,
				*settings->video_codec, settings->video_codec_options, video_info->width, video_info->height, video_info->frame_rate, video_info->pix_fmt,
				*settings->audio_codec, settings->audio_codec_options, audio_info->sample_fmt, audio_info->sample_rate, audio_info->channel_layout);
		}
		else {
			throw std::runtime_error("cannot initialize format: missing settings or info structures");
		}
	}
	LOG_CATCH;
	LOG_EXIT;
	return hr;
}

STDAPI SinkWriterWriteSample(
	IMFSinkWriter *pThis,
	DWORD         dwStreamIndex,
	IMFSample     *pSample)
{
	LOG_ENTER;
	try {
		// write our audio or video sample; note: this will clear the sample as well
		if (!format) {
			throw std::runtime_error("format not initialized");
		}
		winrt::com_ptr<IMFMediaBuffer> p_media_buffer = nullptr;
		BYTE* p_buffer = nullptr;
		DWORD buffer_length = 0;
		THROW_FAILED(pSample->ConvertToContiguousBuffer(p_media_buffer.put()));
		THROW_FAILED(p_media_buffer->Lock(&p_buffer, NULL, &buffer_length));
		if (audio_info && dwStreamIndex == audio_info->stream_index) {
			LOG->debug("transcoding {} bytes to audio stream", buffer_length);
			int bytes_per_sample = av_get_bytes_per_sample(audio_info->sample_fmt);
			int nb_channels = av_get_channel_layout_nb_channels(audio_info->channel_layout);
			int nb_samples = buffer_length / (bytes_per_sample * nb_channels);
			if (buffer_length != nb_samples * bytes_per_sample * nb_channels) {
				LOG->error("buffer length {} not a multiple of bytes per sample {} and number of channels {}",
					buffer_length, bytes_per_sample, nb_channels);
			}
			else {
				auto frame = CreateAudioFrame(
					audio_info->sample_fmt, audio_info->sample_rate, audio_info->channel_layout, nb_samples, p_buffer);
				std::lock_guard<std::mutex> lock(format_mutex);
				format->astream.Transcode(frame);
			}
		}
		if (video_info && dwStreamIndex == video_info->stream_index) {
			LOG->debug("transcoding {} bytes to video stream", buffer_length);
			auto pix_desc = av_pix_fmt_desc_get(video_info->pix_fmt);
			auto bits_per_pixel = av_get_padded_bits_per_pixel(pix_desc);
			if (buffer_length * 8 != video_info->width * video_info->height * bits_per_pixel) {
				LOG->error("buffer length {} does not match video frame of size {}x{} at {} bits per pixel",
					buffer_length, video_info->width, video_info->height, bits_per_pixel);
			}
			else {
				auto frame = CreateVideoFrame(
					video_info->width, video_info->height, video_info->pix_fmt, p_buffer);
				std::lock_guard<std::mutex> lock(format_mutex);
				format->vstream.Transcode(frame);
			}
		}
		memset(p_buffer, 0, buffer_length); // clear sample so game will output blank video/audio
		THROW_FAILED(p_media_buffer->Unlock());
	}
	LOG_CATCH;
	auto hr = E_FAIL;
	try {
		// call original function
		if (!sinkwriter_hook)
			throw std::runtime_error("IMFSinkWriter hook not set up");
		hr = sinkwriter_hook->orig_func<VSinkWriterWriteSample>(pThis, dwStreamIndex, pSample);
		THROW_FAILED(hr);
	}
	LOG_CATCH;
	LOG_EXIT;
	return hr;
}

STDAPI SinkWriterFlush(
	IMFSinkWriter* pThis,
	DWORD         dwStreamIndex)
{
	LOG_ENTER;
	try {
		LOG->info("flushing transcoder");
		if (format) {
			std::lock_guard<std::mutex> lock(format_mutex);
			format->Flush();
			format = nullptr;
		}
	}
	LOG_CATCH;
	auto hr = E_FAIL;
	try {
		if (!sinkwriter_hook)
			throw std::runtime_error("IMFSinkWriter hook not set up");
		hr = sinkwriter_hook->orig_func<VSinkWriterFlush>(pThis, dwStreamIndex);
		/* we should no longer use this IMFSinkWriter instance, so clean up all virtual function hooks */
		UnhookVFuncDetours();
		THROW_FAILED(hr);
		LOG->info("export cancelled");
	}
	LOG_CATCH;
	LOG_EXIT;
	return hr;
}

STDAPI SinkWriterFinalize(
	IMFSinkWriter *pThis)
{
	LOG_ENTER;
	auto hr = E_FAIL;
	try {
		LOG->info("flushing transcoder");
		if (format) {
			std::lock_guard<std::mutex> lock(format_mutex);
			format->Flush();
			format = nullptr;
		}
		if (!sinkwriter_hook)
			throw std::runtime_error("IMFSinkWriter hook not set up");
		hr = sinkwriter_hook->orig_func<VSinkWriterFinalize>(pThis);
		/* we should no longer use this IMFSinkWriter instance, so clean up all virtual function hooks */
		UnhookVFuncDetours();
		THROW_FAILED(hr);
		LOG->info("export finished");
	}
	LOG_CATCH;
	LOG_EXIT;
	return hr;
}

STDAPI CreateSinkWriterFromURL(
	LPCWSTR       pwszOutputURL,
	IMFByteStream *pByteStream,
	IMFAttributes *pAttributes,
	IMFSinkWriter **ppSinkWriter
	)
{
	LOG_ENTER;
	auto hr = E_FAIL;
	try {
		// unhook any dangling detours
		UnhookVFuncDetours();
		// now start export
		LOG->info("export started");
		if (!create_sinkwriter_hook) {
			LOG->error("MFCreateSinkWriterFromURL: hook not set up");
			LOG_EXIT;
			return E_FAIL;
		}
		LOG->trace("MFCreateSinkWriterFromURL: enter");
		hr = create_sinkwriter_hook->origFunc(pwszOutputURL, pByteStream, pAttributes, ppSinkWriter);
		LOG->trace("MFCreateSinkWriterFromURL: exit {}", hr);
		// reload settings to see if the mod is enabled, and to get the latest settings
		settings = std::make_unique<Settings>();
		auto enable = true;
		auto exportsec = GetSec(settings->sections, "export");
		GetVar(exportsec, "enable", enable);
		if (!enable) {
			LOG->info("mod disabled, default in-game video export will be used");
		}
		else if (FAILED(hr)) {
			LOG->info("MFCreateSinkWriterFromURL failed");
		}
		else {
			if (ppSinkWriter == nullptr)
				throw std::runtime_error("ppSinkWriter is null");
			if (*ppSinkWriter == nullptr)
				throw std::runtime_error("*ppSinkWriter is null");
			PLH::VFuncMap redirect_map = make_redirect_map(
				VSinkWriterSetInputMediaType(&SinkWriterSetInputMediaType),
				VSinkWriterBeginWriting(&SinkWriterBeginWriting),
				VSinkWriterWriteSample(&SinkWriterWriteSample),
				VSinkWriterFlush(&SinkWriterFlush),
				VSinkWriterFinalize(&SinkWriterFinalize)
			);
			sinkwriter_hook = std::make_unique<VTableSwapHook>(
				static_cast<IUnknown*>(*ppSinkWriter), redirect_map
			);
		}
	}
	LOG_CATCH;
	LOG_EXIT;
	return hr;
}

void Hook()
{
	LOG_ENTER;
	try {
		UnhookVFuncDetours(); // virtual functions are hooked by CreateSinkWriterFromURL
		create_sinkwriter_hook.reset(new IatHook("mfreadwrite.dll", "MFCreateSinkWriterFromURL", &CreateSinkWriterFromURL));
	}
	LOG_CATCH;
	LOG_EXIT;
}
