/* hooks into media foundation's sink writer for intercepting video/audio data */

#include "sinkwriter.h"
#include "logger.h"
#include "settings.h"
#include "hook.h"
#include "filehandle.h"

#include <wrl/client.h> // ComPtr

#include <mfapi.h>
#include <mfplay.h>
#include <mfreadwrite.h>
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")

using namespace Microsoft::WRL;

std::string GUIDToString(const GUID & guid) {
	char buffer[48];
	snprintf(buffer, sizeof(buffer), "%08lX-%04hX-%04hX-%02hhX%02hhX-%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX",
		guid.Data1, guid.Data2, guid.Data3,
		guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
		guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);
	return std::string(buffer);
}

struct AudioInfo {
	DWORD stream_index;
	FileHandle os;
};

struct VideoInfo {
	DWORD stream_index;
	UINT32 width;
	UINT32 height;
	UINT32 framerate_numerator;
	UINT32 framerate_denominator;
	FileHandle os;
};

std::unique_ptr<PLH::IATHook> sinkwriter_hook = nullptr;
std::unique_ptr<PLH::VFuncDetour> setinputmediatype_hook = nullptr;
std::unique_ptr<PLH::VFuncDetour> writesample_hook = nullptr;
std::unique_ptr<PLH::VFuncDetour> finalize_hook = nullptr;
std::unique_ptr<AudioInfo> audio_info = nullptr;
std::unique_ptr<VideoInfo> video_info = nullptr;

void UnhookVFuncDetours()
{
	LOG_ENTER;
	setinputmediatype_hook = nullptr;
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

std::unique_ptr<AudioInfo> GetAudioInfo(DWORD stream_index, IMFMediaType *input_media_type) {
	LOG_ENTER;
	std::unique_ptr<AudioInfo> info(new AudioInfo);
	LOG->debug("audio stream index = {}", stream_index);
	info->stream_index = stream_index;
	GUID subtype = { 0 };
	auto hr = input_media_type->GetGUID(MF_MT_SUBTYPE, &subtype);
	if (SUCCEEDED(hr)) {
		if (subtype == MFAudioFormat_PCM) {
			LOG->info("audio format = PCM ({})", GUIDToString(subtype));
		}
		else {
			hr = E_FAIL;
			LOG->error("audio format unsupported");
		}
	}
	if (SUCCEEDED(hr)) {
		info->os = FileHandle("audio.raw");
		hr = (info->os.IsValid() ? S_OK : E_FAIL);
	}
	if (FAILED(hr)) {
		info = nullptr;
	}
	LOG_EXIT;
	return info;
}

std::unique_ptr<VideoInfo> GetVideoInfo(DWORD stream_index, IMFMediaType *input_media_type) {
	LOG_ENTER;
	std::unique_ptr<VideoInfo> info(new VideoInfo);
	LOG->debug("video stream index = {}", stream_index);
	info->stream_index = stream_index;
	GUID subtype = { 0 };
	auto hr = input_media_type->GetGUID(MF_MT_SUBTYPE, &subtype);
	if (SUCCEEDED(hr)) {
		if (subtype == MFVideoFormat_NV12) {
			LOG->info("video format = NV12 ({})", GUIDToString(subtype));
		}
		else {
			hr = E_FAIL;
			LOG->error("video format unsupported");
		}
	}
	hr = MFGetAttributeSize(input_media_type, MF_MT_FRAME_SIZE, &info->width, &info->height);
	if (SUCCEEDED(hr)) {
		LOG->info("video size = {}x{}", info->width, info->height);
	}
	hr = MFGetAttributeRatio(input_media_type, MF_MT_FRAME_RATE, &info->framerate_numerator, &info->framerate_denominator);
	if (SUCCEEDED(hr)) {
		LOG->info("video framerate = {}/{}", info->framerate_numerator, info->framerate_denominator);
	}
	if (SUCCEEDED(hr)) {
		info->os = FileHandle("video.yuv");
		hr = (info->os.IsValid() ? S_OK : E_FAIL);
	}
	if (FAILED(hr)) {
		info = nullptr;
	}
	LOG_EXIT;
	return info;
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
			audio_info = GetAudioInfo(dwStreamIndex, pInputMediaType);
		}
		else if (major_type == MFMediaType_Video) {
			video_info = GetVideoInfo(dwStreamIndex, pInputMediaType);
		}
		else {
			LOG->debug("unknown stream at index {}", dwStreamIndex);
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
	if (audio_info) {
		if (dwStreamIndex == audio_info->stream_index) {
			WriteSample(pSample, audio_info->os.Handle());
		}
	}
	if (video_info) {
		if (dwStreamIndex == video_info->stream_index) {
			WriteSample(pSample, video_info->os.Handle());
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
	if (!settings->enable_) {
		LOG->info("mod disabled, default in-game video export will be used");
		UnhookVFuncDetours();
	}
	else if (FAILED(hr)) {
		LOG->info("MFCreateSinkWriterFromURL failed");
		UnhookVFuncDetours();
	} else {
		setinputmediatype_hook = CreateVFuncDetour(*ppSinkWriter, 4, &SinkWriterSetInputMediaType);
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
