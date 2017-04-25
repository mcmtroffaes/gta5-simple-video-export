/* hooks into media foundation's sink writer for intercepting video/audio data */

#include "sinkwriter.h"
#include "logger.h"
#include "hook.h"

#include <mfplay.h>
#include <mfreadwrite.h>

#pragma comment(lib, "mfreadwrite.lib")

std::unique_ptr<PLH::IATHook> sinkwriter_hook = nullptr;
std::unique_ptr<PLH::VFuncDetour> finalize_hook = nullptr;

HRESULT __stdcall FinalizeNew(
	IMFSinkWriter *pThis
	) {
	LOG_ENTER;
	if (!finalize_hook) {
		LOG_ERROR("finalize_hook not set up");
		return E_FAIL;
	}
	logger->trace("IMFSinkWriter::Finalize: enter");
	auto hr = finalize_hook->GetOriginal<decltype(&FinalizeNew)>()(pThis);
	logger->trace("IMFSinkWriter::Finalize: exit");
	/* we will no longer use this IMFSinkWriter instance, so clean up all hooks into it */
	finalize_hook = nullptr;
	sinkwriter_hook = nullptr;
	logger->debug("cleared all IMFSinkWriter hooks");
	LOG_EXIT;
	return hr;
}

HRESULT __stdcall SinkWriterNew(
	LPCWSTR       pwszOutputURL,
	IMFByteStream *pByteStream,
	IMFAttributes *pAttributes,
	IMFSinkWriter **ppSinkWriter
	)
{
	LOG_ENTER;
	if (!sinkwriter_hook) {
		LOG_ERROR("sinkwriter_orig_func not initialized; hook not set up?");
		return E_FAIL;
	}
	logger->trace("MFCreateSinkWriterFromURL: enter");
	auto hr = sinkwriter_hook->GetOriginal<decltype(&SinkWriterNew)>()(pwszOutputURL, pByteStream, pAttributes, ppSinkWriter);
	logger->trace("MFCreateSinkWriterFromURL: exit");
	if (SUCCEEDED(hr)) {
		LOG_TRACE("hooking IMFSinkWriter::Finalize");
		finalize_hook = CreateVFuncDetour(*ppSinkWriter, 11, &FinalizeNew);
	}
	LOG_EXIT;
	return hr;
}

void Hook()
{
	LOG_ENTER;
	finalize_hook = nullptr;
	sinkwriter_hook = CreateIATHook("mfreadwrite.dll", "MFCreateSinkWriterFromURL", &SinkWriterNew);
	LOG_EXIT;
}

void Unhook()
{
	LOG_ENTER;
	sinkwriter_hook = nullptr;
	finalize_hook = nullptr;
	LOG_EXIT;
}
