#include "sinkwriter.h"
#include "logger.h"
#include "../PolyHook/PolyHook/PolyHook.hpp"

#include <mfplay.h>
#include <mfreadwrite.h>

#pragma comment(lib, "mfreadwrite.lib")

template <class FuncType>
std::unique_ptr<PLH::IATHook> CreateIATHook(const char *library_name, const char *func_name, FuncType new_func) {
	LOG_ENTER;
	std::unique_ptr<PLH::IATHook> hook(new PLH::IATHook());
	hook->SetupHook(library_name, func_name, (BYTE*)new_func);
	if (hook->Hook()) {
		logger->debug("hook set for {}", func_name);
	}
	else {
		hook = nullptr;
		logger->debug("failed to hook {}", func_name);
	}
	return hook;
	LOG_EXIT;
}

/* helper function to create a hook for a virtual function */
template <class ClassType, class FuncType>
std::unique_ptr<PLH::VFuncDetour> CreateVFuncDetour(ClassType *p_instance, int vfunc_index, FuncType new_func) {
	LOG_ENTER;
	std::unique_ptr<PLH::VFuncDetour> hook(new PLH::VFuncDetour());
	if (!p_instance) {
		hook = nullptr;
		LOG_ERROR("hook failed: class instance is null");
	}
	else {
		hook->SetupHook(*(BYTE***)p_instance, vfunc_index, (BYTE*)new_func);
		if (hook->Hook()) {
			LOG_DEBUG("hook set");
		}
		else {
			hook = nullptr;
			LOG_ERROR("hook failed");
		}
	}
	LOG_EXIT;
	return hook;
}


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
