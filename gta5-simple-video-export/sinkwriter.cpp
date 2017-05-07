/* hooks into media foundation's sink writer for intercepting video/audio data */

#include "sinkwriter.h"
#include "logger.h"
#include "settings.h"
#include "hook.h"
#include "info.h"

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
std::unique_ptr<GeneralInfo> info = nullptr;

void UnhookVFuncDetours()
{
	LOG_ENTER;
	setinputmediatype_hook = nullptr;
	beginwriting_hook = nullptr;
	writesample_hook = nullptr;
	finalize_hook = nullptr;
	audio_info = nullptr;
	video_info = nullptr;
	info = nullptr;
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
		auto hr2 = pInputMediaType->GetMajorType(&major_type);
		if (FAILED(hr2)) {
			LOG->error("failed to get major type for stream at index {}", dwStreamIndex);
		}
		else if (major_type == MFMediaType_Audio) {
			if (settings && info) {
				audio_info.reset(new AudioInfo(dwStreamIndex, *pInputMediaType, *settings, *info));
			}
		}
		else if (major_type == MFMediaType_Video) {
			if (settings && info) {
				video_info.reset(new VideoInfo(dwStreamIndex, *pInputMediaType, *settings, *info));
			}
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
	if (settings && info && audio_info && video_info) {
		CreateClientBatchFile(*settings, *info, *audio_info, *video_info);
		if (settings->IsRawFolderPipe()) {
			// TODO run client command and wait for audio and video pipes to be connected
		}
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
	// we don't need to call the original
	/*
	if (!writesample_hook) {
		LOG->error("IMFSinkWriter::WriteSample hook not set up");
		return E_FAIL;
	}
	auto original_func = writesample_hook->GetOriginal<decltype(&SinkWriterWriteSample)>();
	LOG->trace("IMFSinkWriter::WriteSample: enter");
	auto hr = original_func(pThis, dwStreamIndex, pSample);
	LOG->trace("IMFSinkWriter::WriteSample: exit {}", hr);
	*/
	if (audio_info && audio_info->os_ && audio_info->os_->IsValid()) {
		if (dwStreamIndex == audio_info->stream_index_) {
			WriteSample(pSample, audio_info->os_->Handle());
		}
	}
	if (video_info && video_info->os_ && video_info->os_->IsValid()) {
		if (dwStreamIndex == video_info->stream_index_) {
			WriteSample(pSample, video_info->os_->Handle());
		}
	}
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
	info.reset(new GeneralInfo);
	if (!settings->enable_) {
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
