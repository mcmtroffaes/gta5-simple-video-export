/*
Hooks into media foundation's sink writer for intercepting video/audio data.

The main entry point is CreateSinkWriterFromURL. This reloads the ini file (so
we always have the latest settings; coincidently this also updates the
timestamp), checks if the mod is enabled, and sets up all the required
SinkWriter hooks.

Under normal circumstances, the game will then call SinkWriterSetInputMediaType
twice, once for the audio, and once for the video. At this point, we intercept
the audio and video information (resolution, format, ...).

After that, the game will call SinkWriterBeginWriting. There, we propagate all
the information gathered so far into the ini file (via UpdateSettings), we
interpolate the ini file, we create the file handles that will contain the
raw exported audio and video, and we create the batch file for post-processing.

Next, the game will repeatedly call SinkWriterWriteSample. We intercept the
raw data and write it to our own file handles.

At the end of the export process, the game calls SinkWriterFinalize. There we
unhook all the SinkWriter hooks (this will also close all files).
*/

#include "sinkwriter.h"
#include "logger.h"
#include "settings.h"
#include "hook.h"
#include "info.h"
#include "format.h"

#include <wrl/client.h> // ComPtr

#include <mfapi.h>
#include <mfplay.h>
#include <mfreadwrite.h>
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")

using namespace Microsoft::WRL;

std::unique_ptr<PLH::IATHook> sinkwriter_hook = nullptr;
std::unique_ptr<PLH::VFuncDetour> setinputmediatype_hook = nullptr;
std::unique_ptr<PLH::VFuncDetour> beginwriting_hook = nullptr;
std::unique_ptr<PLH::VFuncDetour> writesample_hook = nullptr;
std::unique_ptr<PLH::VFuncDetour> finalize_hook = nullptr;
std::unique_ptr<AudioInfo> audio_info = nullptr;
std::unique_ptr<VideoInfo> video_info = nullptr;
std::unique_ptr<Format> format = nullptr;

void UnhookVFuncDetours()
{
	LOG_ENTER;
	setinputmediatype_hook = nullptr;
	beginwriting_hook = nullptr;
	writesample_hook = nullptr;
	finalize_hook = nullptr;
	audio_info = nullptr;
	video_info = nullptr;
	format = nullptr;
	LOG_EXIT;
}

void Unhook()
{
	LOG_ENTER;
	UnhookVFuncDetours();
	sinkwriter_hook = nullptr;
	LOG_EXIT;
}

STDAPI SinkWriterSetInputMediaType(
	IMFSinkWriter *pThis,
	DWORD         dwStreamIndex,
	IMFMediaType  *pInputMediaType,
	IMFAttributes *pEncodingParameters)
{
	LOG_ENTER;
	if (!setinputmediatype_hook) {
		LOG->error("IMFSinkWriter::SetInputMediaType hook not set up");
		LOG_EXIT;
		return E_FAIL;
	}
	auto original_func = setinputmediatype_hook->GetOriginal<decltype(&SinkWriterSetInputMediaType)>();
	LOG->trace("IMFSinkWriter::SetInputMediaType: enter");
	auto hr = original_func(pThis, dwStreamIndex, pInputMediaType, pEncodingParameters);
	LOG->trace("IMFSinkWriter::SetInputMediaType: exit {}", hr);
	if (SUCCEEDED(hr)) {
		GUID major_type = { 0 };
		auto hr2 = pInputMediaType ? S_OK : E_FAIL;
		if (FAILED(hr2)) {
			LOG->error("input media type pointer is null");
		}
		else {
			hr2 = pInputMediaType->GetMajorType(&major_type);
		}
		if (FAILED(hr2)) {
			LOG->error("failed to get major type for stream at index {}", dwStreamIndex);
		}
		else if (major_type == MFMediaType_Audio) {
			audio_info.reset(new AudioInfo(dwStreamIndex, *pInputMediaType));
		}
		else if (major_type == MFMediaType_Video) {
			video_info.reset(new VideoInfo(dwStreamIndex, *pInputMediaType));
		}
		else {
			LOG->debug("unknown stream at index {}", dwStreamIndex);
		}
	}
	LOG_EXIT;
	return hr;
}

STDAPI SinkWriterBeginWriting(
	IMFSinkWriter *pThis)
{
	LOG_ENTER;
	if (!beginwriting_hook) {
		LOG->error("IMFSinkWriter::BeginWriting hook not set up");
		return E_FAIL;
	}
	auto original_func = beginwriting_hook->GetOriginal<decltype(&SinkWriterBeginWriting)>();
	LOG->trace("IMFSinkWriter::BeginWriting: enter");
	auto hr = original_func(pThis);
	LOG->trace("IMFSinkWriter::BeginWriting: exit {}", hr);
	if (settings && audio_info && video_info) {
		std::ostringstream os;
		settings->generate(os);
		LOG->debug("settings before interpolation:\n{}", os.str());
		settings->interpolate();
		os.str("");
		settings->generate(os);
		LOG->debug("settings after interpolation:\n{}", os.str());
		format.reset(new Format(
			settings->export_filename,
			settings->video_codec_id, video_info->width, video_info->height, video_info->frame_rate, video_info->pix_fmt,
			settings->audio_codec_id, audio_info->sample_fmt, audio_info->sample_rate, audio_info->channel_layout));
	}
	LOG_EXIT;
	return hr;
}

STDAPI SinkWriterWriteSample(
	IMFSinkWriter *pThis,
	DWORD         dwStreamIndex,
	IMFSample     *pSample)
{
	LOG_ENTER;
	// write our audio or video sample; note: this will clear the sample as well
	ComPtr<IMFMediaBuffer> p_media_buffer = nullptr;
	BYTE *p_buffer = nullptr;
	DWORD buffer_length = 0;
	auto hr = pSample->ConvertToContiguousBuffer(p_media_buffer.GetAddressOf());
	if (SUCCEEDED(hr))
	{
		hr = p_media_buffer->Lock(&p_buffer, NULL, &buffer_length);
	}
	if (SUCCEEDED(hr))
	{
		if (audio_info && dwStreamIndex == audio_info->stream_index) {
			LOG->debug("transcoding {} bytes to audio stream", buffer_length);
			int bytes_per_sample = av_get_bytes_per_sample(audio_info->sample_fmt);
			int nb_channels = av_get_channel_layout_nb_channels(audio_info->channel_layout);
			int nb_samples = buffer_length / (bytes_per_sample * nb_channels);
			if (buffer_length != nb_samples * bytes_per_sample * nb_channels)
				LOG->warn("buffer length {} not a multiple of bytes per sample {} and number of channels {}",
					buffer_length, bytes_per_sample, nb_channels);
			auto frame = CreateAudioFrame(
				audio_info->sample_fmt, audio_info->sample_rate, audio_info->channel_layout, nb_samples, p_buffer);
			format->astream->Transcode(frame);
		}
		if (video_info && dwStreamIndex == video_info->stream_index) {
			LOG->debug("transcoding {} bytes to video stream", buffer_length);
			// TODO check buffer length
			auto frame = CreateVideoFrame(
				video_info->width, video_info->height, video_info->pix_fmt, p_buffer);
			format->vstream->Transcode(frame);
		}
		memset(p_buffer, 0, buffer_length); // clear sample so game will output blank video/audio
		hr = p_media_buffer->Unlock();
	}
	// call original function
	if (!writesample_hook) {
		LOG->error("IMFSinkWriter::WriteSample hook not set up");
		return E_FAIL;
	}
	auto original_func = writesample_hook->GetOriginal<decltype(&SinkWriterWriteSample)>();
	LOG->trace("IMFSinkWriter::WriteSample: enter");
	hr = original_func(pThis, dwStreamIndex, pSample);
	LOG->trace("IMFSinkWriter::WriteSample: exit {}", hr);
	LOG_EXIT;
	return hr;
}

STDAPI SinkWriterFinalize(
	IMFSinkWriter *pThis)
{
	LOG_ENTER;
	LOG->info("flushing transcoder");
	format->Flush();
	if (!finalize_hook) {
		LOG->error("IMFSinkWriter::Finalize hook not set up");
		return E_FAIL;
	}
	auto original_func = finalize_hook->GetOriginal<decltype(&SinkWriterFinalize)>();
	LOG->trace("IMFSinkWriter::Finalize: enter");
	auto hr = original_func(pThis);
	LOG->trace("IMFSinkWriter::Finalize: exit {}", hr);
	/* we should no longer use this IMFSinkWriter instance, so clean up all virtual function hooks */
	UnhookVFuncDetours();
	LOG->info("export finished");
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
	LOG->info("export started");
	if (!sinkwriter_hook) {
		LOG->error("MFCreateSinkWriterFromURL hook not set up");
		LOG_EXIT;
		return E_FAIL;
	}
	auto original_func = sinkwriter_hook->GetOriginal<decltype(&CreateSinkWriterFromURL)>();
	LOG->trace("MFCreateSinkWriterFromURL: enter");
	auto hr = original_func(pwszOutputURL, pByteStream, pAttributes, ppSinkWriter);
	LOG->trace("MFCreateSinkWriterFromURL: exit {}", hr);
	// reload settings to see if the mod is enabled, and to get the latest settings
	settings.reset(new Settings);
	auto enable = true;
	auto defsec = settings->GetSec("builtin");
	settings->GetVar(defsec, "enable", enable);
	if (!enable) {
		LOG->info("mod disabled, default in-game video export will be used");
		UnhookVFuncDetours();
	}
	else if (FAILED(hr)) {
		LOG->info("MFCreateSinkWriterFromURL failed");
		UnhookVFuncDetours();
	} else {
		setinputmediatype_hook = CreateVFuncDetour(*ppSinkWriter, 4, &SinkWriterSetInputMediaType);
		beginwriting_hook = CreateVFuncDetour(*ppSinkWriter, 5, &SinkWriterBeginWriting);
		writesample_hook = CreateVFuncDetour(*ppSinkWriter, 6, &SinkWriterWriteSample);
		finalize_hook = CreateVFuncDetour(*ppSinkWriter, 11, &SinkWriterFinalize);
	}
	LOG_EXIT;
	return hr;
}

void Hook()
{
	LOG_ENTER;
	UnhookVFuncDetours(); // virtual functions are hooked by CreateSinkWriterFromURL
	sinkwriter_hook = CreateIATHook("mfreadwrite.dll", "MFCreateSinkWriterFromURL", &CreateSinkWriterFromURL);
	LOG_EXIT;
}
