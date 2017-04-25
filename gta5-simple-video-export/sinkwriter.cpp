/* hooks into media foundation's sink writer for intercepting video/audio data */

#include "sinkwriter.h"
#include "logger.h"
#include "hook.h"

#include <mfplay.h>
#include <mfreadwrite.h>

#pragma comment(lib, "mfreadwrite.lib")

std::unique_ptr<PLH::IATHook> sinkwriter_hook = nullptr;
std::unique_ptr<PLH::VFuncDetour> finalize_hook = nullptr;

void UnhookVFuncDetours()
{
	LOG_ENTER;
	finalize_hook = nullptr;
	LOG_EXIT;
}

void Unhook()
{
	LOG_ENTER;
	UnhookVFuncDetours();
	sinkwriter_hook = nullptr;
	LOG_EXIT;
}

HRESULT __stdcall SinkWriterFinalize(
	IMFSinkWriter *pThis
	) {
	LOG_ENTER;
	if (!finalize_hook) {
		logger->error("IMFSinkWriter::Finalize hook not set up");
		return E_FAIL;
	}
	auto original_func = finalize_hook->GetOriginal<decltype(&SinkWriterFinalize)>();
	logger->trace("IMFSinkWriter::Finalize ({:x}): enter", (void *)original_func);
	auto hr = original_func(pThis);
	logger->trace("IMFSinkWriter::Finalize ({:x}): exit", (void *)original_func);
	/* we should no longer use this IMFSinkWriter instance, so clean up all virtual function hooks */
	UnhookVFuncDetours();
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
		logger->error("MFCreateSinkWriterFromURL hook not set up");
		return E_FAIL;
	}
	auto original_func = sinkwriter_hook->GetOriginal<decltype(&CreateSinkWriterFromURL)>();
	logger->trace("MFCreateSinkWriterFromURL ({:x}): enter", (void *)original_func);
	auto hr = original_func(pwszOutputURL, pByteStream, pAttributes, ppSinkWriter);
	logger->trace("MFCreateSinkWriterFromURL ({:x}): exit", (void *)original_func);
	if (SUCCEEDED(hr)) {
		finalize_hook = CreateVFuncDetour(*ppSinkWriter, 11, &SinkWriterFinalize);
	}
	LOG_EXIT;
	return hr;
}

void Hook()
{
	LOG_ENTER;
	UnhookVFuncDetours(); // these are hooked by CreateSinkWriterFromURL
	sinkwriter_hook = CreateIATHook("mfreadwrite.dll", "MFCreateSinkWriterFromURL", &CreateSinkWriterFromURL);
	LOG_EXIT;
}
