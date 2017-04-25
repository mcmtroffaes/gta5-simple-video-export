#include "sinkwriter.h"
#include "logger.h"
#include "../PolyHook/PolyHook/PolyHook.hpp"

#include <mfplay.h>
#include <mfreadwrite.h>

#pragma comment(lib, "mfreadwrite.lib")

/* helper function to create a hook for a virtual function */
template <class ClassType, class FuncType>
std::unique_ptr<PLH::VFuncDetour> CreateVFuncDetour(ClassType *p_instance, int vfunc_index, LPVOID new_func, FuncType *orig_func) {
	LOG_ENTER;
	std::unique_ptr<PLH::VFuncDetour> hook(new PLH::VFuncDetour());
	if (!p_instance) {
		hook = nullptr;
		*orig_func = nullptr;
		LOG_ERROR("hook failed (instance is null)");
	}
	else {
		hook->SetupHook(*(BYTE***)p_instance, vfunc_index, (BYTE*)new_func);
		if (hook->Hook()) {
			*orig_func = hook->GetOriginal<FuncType>();
			LOG_DEBUG("hook set");
		}
		else {
			hook = nullptr;
			*orig_func = nullptr;
			LOG_ERROR("hook failed (polyhook error)");
		}
	}
	LOG_EXIT;
	return hook;
}

typedef HRESULT (__stdcall *FinalizeFuncType)(
	IMFSinkWriter *pThis
	);

std::unique_ptr<PLH::IATHook> sinkwriter_hook = nullptr;
std::unique_ptr<PLH::VFuncDetour> finalize_hook = nullptr;
decltype(&MFCreateSinkWriterFromURL) sinkwriter_orig_func = nullptr;
FinalizeFuncType finalize_orig_func = nullptr;

HRESULT __stdcall FinalizeNew(
	IMFSinkWriter *pThis
	) {
	LOG_ENTER;
	if (!finalize_orig_func) {
		LOG_ERROR("finalize_orig_func not initialized; hook not set up?");
		return E_FAIL;
	}
	logger->trace("finalize_orig_func: enter");
	auto hr = finalize_orig_func(pThis);
	logger->trace("finalize_orig_func: exit");
	/* we will no longer use this IMFSinkWriter instance, so clean up all hooks into it */
	finalize_hook = nullptr;
	finalize_orig_func = nullptr;
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
	if (!sinkwriter_orig_func) {
		LOG_ERROR("sinkwriter_orig_func not initialized; hook not set up?");
		return E_FAIL;
	}
	logger->trace("sinkwriter_orig_func: enter");
	auto hr = sinkwriter_orig_func(pwszOutputURL, pByteStream, pAttributes, ppSinkWriter);
	logger->trace("sinkwriter_orig_func: exit");
	if (SUCCEEDED(hr)) {
		LOG_TRACE("hooking IMFSinkWriter::Finalize");
		finalize_hook = CreateVFuncDetour(*ppSinkWriter, 11, &FinalizeNew, &finalize_orig_func);
	}
	LOG_EXIT;
	return hr;
}

void Hook()
{
	LOG_ENTER;
	sinkwriter_hook.reset(new PLH::IATHook);
	sinkwriter_hook->SetupHook("mfreadwrite.dll", "MFCreateSinkWriterFromURL", (BYTE*)&SinkWriterNew);
	if (sinkwriter_hook->Hook()) {
		sinkwriter_orig_func = sinkwriter_hook->GetOriginal<decltype(&MFCreateSinkWriterFromURL)>();
		LOG_DEBUG("MFCreateSinkWriterFromURL hooked");
	}
	else {
		sinkwriter_hook = nullptr;
		LOG_ERROR("MFCreateSinkWriterFromURL hook failed");
	}
	LOG_EXIT;
}

void Unhook()
{
	LOG_ENTER;
	sinkwriter_hook = nullptr;
	finalize_hook = nullptr;
	sinkwriter_orig_func = nullptr;
	finalize_orig_func = nullptr;
	LOG_EXIT;
}
