/* hooks into media foundation's sink writer for intercepting video/audio data */

#include "sinkwriter.h"
#include "logger.h"
#include "hook.h"

#include <mfplay.h>
#include <mfreadwrite.h>

#pragma comment(lib, "mfreadwrite.lib")

std::unique_ptr<PLH::IATHook> sinkwriter_hook = nullptr;
std::unique_ptr<PLH::VFuncDetour> finalize_hook = nullptr;

HRESULT __stdcall SinkWriterFinalize(
	IMFSinkWriter *pThis
	) {
	LOG_ENTER;
	if (!finalize_hook) {
		logger->error("hook not set up");
		return E_FAIL;
	}
	logger->trace("IMFSinkWriter::Finalize: enter");
	auto hr = finalize_hook->GetOriginal<decltype(&SinkWriterFinalize)>()(pThis);
	logger->trace("IMFSinkWriter::Finalize: exit");
	/* we will no longer use this IMFSinkWriter instance, so clean up all hooks */
	Unhook();
	LOG_EXIT;
	return hr;
}

HRESULT __stdcall CreateSinkWriterFromURL(
	LPCWSTR       pwszOutputURL,
	IMFByteStream *pByteStream,
	IMFAttributes *pAttributes,
	IMFSinkWriter **ppSinkWriter
	)
{
	LOG_ENTER;
	if (!sinkwriter_hook) {
		logger->error("hook not set up");
		return E_FAIL;
	}
	logger->trace("MFCreateSinkWriterFromURL: enter");
	auto hr = sinkwriter_hook->GetOriginal<decltype(&CreateSinkWriterFromURL)>()(pwszOutputURL, pByteStream, pAttributes, ppSinkWriter);
	logger->trace("MFCreateSinkWriterFromURL: exit");
	if (SUCCEEDED(hr)) {
		finalize_hook = CreateVFuncDetour(*ppSinkWriter, 11, &SinkWriterFinalize);
	}
	LOG_EXIT;
	return hr;
}

void Hook()
{
	LOG_ENTER;
	finalize_hook = nullptr; // will be hooked when instance is created; see CreateSinkWriterFromURL
	sinkwriter_hook = CreateIATHook("mfreadwrite.dll", "MFCreateSinkWriterFromURL", &CreateSinkWriterFromURL);
	LOG_EXIT;
}

void Unhook()
{
	LOG_ENTER;
	finalize_hook = nullptr;
	sinkwriter_hook = nullptr;
	LOG_EXIT;
}
