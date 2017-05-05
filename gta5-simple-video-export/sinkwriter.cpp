/* hooks into media foundation's sink writer for intercepting video/audio data */

#include "sinkwriter.h"
#include "logger.h"
#include "settings.h"
#include "hook.h"

#include <fstream>
#include <wrl/client.h> // ComPtr

#include <mfapi.h>
#include <mfplay.h>
#include <mfreadwrite.h>
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")

#include <Shlwapi.h> // PathCombine
#pragma comment(lib, "Shlwapi.lib")

using namespace Microsoft::WRL;

// a HANDLE which closes itself upon destruction
class FileHandle {
public:
	FileHandle() : handle_(INVALID_HANDLE_VALUE) {}
	FileHandle(HANDLE handle) : handle_(handle) {}
	~FileHandle() {
		LOG_ENTER;
		if (handle_ != INVALID_HANDLE_VALUE) {
			logger->debug("closing handle");
			CloseHandle(handle_);
		}
		LOG_EXIT;
	};
	auto Handle() const { return handle_; }
private:
	HANDLE handle_;
};

static std::unique_ptr<FileHandle> CreateFileHandle(const std::string & filename) {
	LOG_ENTER;
	logger->info("opening file {} for writing", filename);
	auto handle = CreateFileA(filename.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (handle == NULL || handle == INVALID_HANDLE_VALUE) {
		logger->error("failed to create file {}", filename);
		return nullptr;
	}
	else {
		return std::unique_ptr<FileHandle>(new FileHandle(handle));
	}
	LOG_EXIT;
}

static std::unique_ptr<FileHandle> CreatePipeHandle(const std::string & filename) {
	LOG_ENTER;
	logger->info("opening pipe {} for writing", filename);
	auto handle = CreateNamedPipeA(filename.c_str(), PIPE_ACCESS_OUTBOUND, PIPE_TYPE_BYTE, 1, 0, 0, 0, NULL);
	if (handle == INVALID_HANDLE_VALUE) {
		logger->error("failed to create pipe {}", filename);
		return nullptr;
	}
	else {
		return std::unique_ptr<FileHandle>(new FileHandle(handle));
	}
	LOG_EXIT;
}

struct AudioInfo {
	DWORD stream_index;
	std::unique_ptr<FileHandle> os;
};

struct VideoInfo {
	DWORD stream_index;
	UINT32 width;
	UINT32 height;
	UINT32 framerate_numerator;
	UINT32 framerate_denominator;
	std::unique_ptr<FileHandle> os;
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

std::unique_ptr<FileHandle> OpenOutputFile(const std::string & filename) {
	LOG_ENTER;
	std::unique_ptr<FileHandle> filehandle = nullptr;
	char path[MAX_PATH] = "";
	if (PathCombineA(path, settings->output_folder_.c_str(), filename.c_str()) == nullptr) {
		logger->error("could not combine {} and {} to form path of output stream", settings->output_folder_, filename);
	}
	else {
		if (settings->output_folder_.substr(0, 8) == "\\\\.\\pipe") {
			filehandle = CreatePipeHandle(path);
		}
		else {
			filehandle = CreateFileHandle(path);
		}
	}
	LOG_EXIT;
	return filehandle;
}

std::unique_ptr<AudioInfo> GetAudioInfo(DWORD stream_index, IMFMediaType *input_media_type) {
	LOG_ENTER;
	std::unique_ptr<AudioInfo> info(new AudioInfo);
	logger->debug("audio stream index = {}", stream_index);
	info->stream_index = stream_index;
	GUID subtype = { 0 };
	auto hr = input_media_type->GetGUID(MF_MT_SUBTYPE, &subtype);
	if (SUCCEEDED(hr)) {
		if (subtype == MFAudioFormat_PCM) {
			logger->info("audio format = PCM");
		}
		else {
			hr = E_FAIL;
			logger->error("audio format unsupported");
		}
	}
	if (SUCCEEDED(hr)) {
		info->os = OpenOutputFile("audio.raw");
		hr = (info->os ? S_OK : E_FAIL);
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
	logger->debug("video stream index = {}", stream_index);
	info->stream_index = stream_index;
	GUID subtype = { 0 };
	auto hr = input_media_type->GetGUID(MF_MT_SUBTYPE, &subtype);
	if (SUCCEEDED(hr)) {
		if (subtype == MFVideoFormat_NV12) {
			logger->info("video format = NV12");
		}
		else {
			hr = E_FAIL;
			logger->error("video format unsupported");
		}
	}
	hr = MFGetAttributeSize(input_media_type, MF_MT_FRAME_SIZE, &info->width, &info->height);
	if (SUCCEEDED(hr)) {
		logger->info("video size = {}x{}", info->width, info->height);
	}
	hr = MFGetAttributeRatio(input_media_type, MF_MT_FRAME_RATE, &info->framerate_numerator, &info->framerate_denominator);
	if (SUCCEEDED(hr)) {
		logger->info("video framerate = {}/{}", info->framerate_numerator, info->framerate_denominator);
	}
	if (SUCCEEDED(hr)) {
		info->os = OpenOutputFile("video.yuv");
		hr = (info->os ? S_OK : E_FAIL);
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
			audio_info = GetAudioInfo(dwStreamIndex, pInputMediaType);
		}
		else if (major_type == MFMediaType_Video) {
			video_info = GetVideoInfo(dwStreamIndex, pInputMediaType);
		}
		else {
			logger->debug("unknown stream at index {}", dwStreamIndex);
		}
	}
	LOG_EXIT;
	return hr;
}

DWORD WriteSample(IMFSample *sample, std::unique_ptr<FileHandle> & os) {
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
		logger->debug("writing {} bytes", buffer_length);
		DWORD num_bytes_written = 0;
		if (!WriteFile(os->Handle(), (const char *)p_buffer, buffer_length, &num_bytes_written, NULL)) {
			logger->error("writing failed");
		}
		if (num_bytes_written != buffer_length) {
			logger->error("only written {} bytes out of {} bytes", num_bytes_written, buffer_length);
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
		logger->error("IMFSinkWriter::WriteSample hook not set up");
		return E_FAIL;
	}
	auto original_func = writesample_hook->GetOriginal<decltype(&SinkWriterWriteSample)>();
	logger->trace("IMFSinkWriter::WriteSample: enter");
	auto hr = original_func(pThis, dwStreamIndex, pSample);
	logger->trace("IMFSinkWriter::WriteSample: exit {}", hr);
	*/
	if (audio_info) {
		if (dwStreamIndex == audio_info->stream_index) {
			WriteSample(pSample, audio_info->os);
		}
	}
	if (video_info) {
		if (dwStreamIndex == video_info->stream_index) {
			WriteSample(pSample, video_info->os);
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
	logger->info("export finished");
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
	logger->info("export started");
	if (!sinkwriter_hook) {
		logger->error("MFCreateSinkWriterFromURL hook not set up");
		LOG_EXIT;
		return E_FAIL;
	}
	auto original_func = sinkwriter_hook->GetOriginal<decltype(&CreateSinkWriterFromURL)>();
	logger->trace("MFCreateSinkWriterFromURL: enter");
	auto hr = original_func(pwszOutputURL, pByteStream, pAttributes, ppSinkWriter);
	logger->trace("MFCreateSinkWriterFromURL: exit {}", hr);
	settings.reset(new Settings);
	if (!settings->output_folder_.empty()) {
		/* mod enabled; update hooks on success */
		if (SUCCEEDED(hr)) {
			setinputmediatype_hook = CreateVFuncDetour(*ppSinkWriter, 4, &SinkWriterSetInputMediaType);
			writesample_hook = CreateVFuncDetour(*ppSinkWriter, 6, &SinkWriterWriteSample);
			finalize_hook = CreateVFuncDetour(*ppSinkWriter, 11, &SinkWriterFinalize);
		}
	} else {
		/* mod disabled; inform user and remove all hooks */
		logger->info("mod disabled due to empty output_folder");
		UnhookVFuncDetours();
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
