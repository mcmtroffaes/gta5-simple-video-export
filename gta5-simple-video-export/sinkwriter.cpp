/* hooks into media foundation's sink writer for intercepting video/audio data */

#include "sinkwriter.h"
#include "logger.h"
#include "hook.h"

#include <mfapi.h>
#include <mfplay.h>
#include <mfreadwrite.h>

#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")

struct AudioInfo {
	DWORD stream_index;
};

struct VideoInfo {
	DWORD stream_index;
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

std::unique_ptr<AudioInfo> GetAudioMediaType(DWORD stream_index, const IMFMediaType & input_media_type) {
	LOG_ENTER;
	std::unique_ptr<AudioInfo> info(new AudioInfo);
	logger->debug("audio stream index = {}", stream_index);
	info->stream_index = stream_index;
	LOG_EXIT;
	return info;
}

std::unique_ptr<VideoInfo> GetVideoMediaType(DWORD stream_index, const IMFMediaType & input_media_type) {
	LOG_ENTER;
	std::unique_ptr<VideoInfo> info(new VideoInfo);
	logger->debug("video stream index = {}", stream_index);
	info->stream_index = stream_index;
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
		logger->error("IMFSinkWriter::SetInputMediaType hook not set up");
		LOG_EXIT;
		return E_FAIL;
	}
	auto original_func = setinputmediatype_hook->GetOriginal<decltype(&SinkWriterSetInputMediaType)>();
	logger->trace("IMFSinkWriter::SetInputMediaType: enter");
	auto hr = original_func(pThis, dwStreamIndex, pInputMediaType, pEncodingParameters);
	logger->trace("IMFSinkWriter::SetInputMediaType: exit {}", hr);
	if (SUCCEEDED(hr)) {
		GUID major_type = { 0 };
		auto hr2 = pInputMediaType->GetMajorType(&major_type);
		if (FAILED(hr2)) {
			logger->error("failed to get major type for stream at index {}", dwStreamIndex);
		}
		else if (major_type == MFMediaType_Audio) {
			audio_info = GetAudioMediaType(dwStreamIndex, *pInputMediaType);
		}
		else if (major_type == MFMediaType_Video) {
			video_info = GetVideoMediaType(dwStreamIndex, *pInputMediaType);
		}
		else {
			logger->debug("unknown stream at index {}", dwStreamIndex);
		}
	}
	LOG_EXIT;
	return hr;
}

void WriteAudioSample(const IMFSample & sample) {
	LOG_ENTER;
	LOG_EXIT;
}

void WriteVideoSample(const IMFSample & sample) {
	LOG_ENTER;
	LOG_EXIT;
}

STDAPI SinkWriterWriteSample(
	IMFSinkWriter *pThis,
	DWORD         dwStreamIndex,
	IMFSample     *pSample)
{
	LOG_ENTER;
	if (!writesample_hook) {
		logger->error("IMFSinkWriter::WriteSample hook not set up");
		return E_FAIL;
	}
	auto original_func = writesample_hook->GetOriginal<decltype(&SinkWriterWriteSample)>();
	logger->trace("IMFSinkWriter::WriteSample: enter");
	auto hr = original_func(pThis, dwStreamIndex, pSample);
	logger->trace("IMFSinkWriter::WriteSample: exit {}", hr);
	if (audio_info) {
		if (dwStreamIndex == audio_info->stream_index) {
			WriteAudioSample(*pSample);
		}
	}
	if (video_info) {
		if (dwStreamIndex == video_info->stream_index) {
			WriteVideoSample(*pSample);
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
		logger->error("IMFSinkWriter::Finalize hook not set up");
		return E_FAIL;
	}
	auto original_func = finalize_hook->GetOriginal<decltype(&SinkWriterFinalize)>();
	logger->trace("IMFSinkWriter::Finalize: enter");
	auto hr = original_func(pThis);
	logger->trace("IMFSinkWriter::Finalize: exit {}", hr);
	/* we should no longer use this IMFSinkWriter instance, so clean up all virtual function hooks */
	UnhookVFuncDetours();
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
	if (!sinkwriter_hook) {
		logger->error("MFCreateSinkWriterFromURL hook not set up");
		return E_FAIL;
	}
	auto original_func = sinkwriter_hook->GetOriginal<decltype(&CreateSinkWriterFromURL)>();
	logger->trace("MFCreateSinkWriterFromURL: enter");
	auto hr = original_func(pwszOutputURL, pByteStream, pAttributes, ppSinkWriter);
	logger->trace("MFCreateSinkWriterFromURL: exit {}", hr);
	if (SUCCEEDED(hr)) {
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
