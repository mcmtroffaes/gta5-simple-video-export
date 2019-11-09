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
		auto exportsec = settings->GetSec("export");
		std::string preset{ };
		std::string folder{ };
		std::string filebase{ };
		settings->GetVar(exportsec, "preset", preset);
		settings->GetVar(exportsec, "folder", folder);
		settings->GetVar(exportsec, "filebase", filebase);
		auto presetsec = settings->GetSec(preset);
		std::string container{ };
		settings->GetVar(presetsec, "container", container);
		// TODO set up the Format
		format.reset(); // new Format(folder + "\\" + filebase + "." + container, ...)
	}
	LOG_EXIT;
	return hr;
}

DWORD WriteSample(IMFSample *sample, HANDLE handle) {
	LOG_ENTER;
	ComPtr<IMFMediaBuffer> p_media_buffer = nullptr;
	BYTE *p_buffer = nullptr;
	DWORD buffer_length = 0;
	auto hr = sample->ConvertToContiguousBuffer(p_media_buffer.GetAddressOf());
	if (SUCCEEDED(hr))
	{
		auto hr = p_media_buffer->Lock(&p_buffer, NULL, &buffer_length);
	}
	if (SUCCEEDED(hr))
	{
		LOG->debug("writing {} bytes", buffer_length);
		DWORD num_bytes_written = 0;
		if (!WriteFile(handle, (const char *)p_buffer, buffer_length, &num_bytes_written, NULL)) {
			LOG->error("writing failed");
		}
		if (num_bytes_written != buffer_length) {
			LOG->error("only written {} bytes out of {} bytes", num_bytes_written, buffer_length);
		}
		memset(p_buffer, 0, buffer_length); // clear sample so game will output blank video/audio
		hr = p_media_buffer->Unlock();
	}
	return buffer_length;
	LOG_EXIT;
}

STDAPI SinkWriterWriteSample(
	IMFSinkWriter *pThis,
	DWORD         dwStreamIndex,
	IMFSample     *pSample)
{
	LOG_ENTER;
	auto hr = S_OK;
	// write our audio or video sample; note: this will clear the sample as well
	if (audio_info) {
		if (dwStreamIndex == audio_info->stream_index_) {
			WriteSample(pSample, nullptr);
		}
	}
	if (video_info) {
		if (dwStreamIndex == video_info->stream_index_) {
			WriteSample(pSample, nullptr);
		}
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
