#include "..\PolyHook\PolyHook\PolyHook.hpp"
#include "..\spdlog\include\spdlog\spdlog.h"
#include <mfplay.h>
#include <mfreadwrite.h>

#define SCRIPT_NAME "simple-video-export"
#define SCRIPT_FOLDER "SVE"

std::shared_ptr<spdlog::logger> logger;
std::shared_ptr<PLH::IATHook> sinkwriter_hook;

#pragma comment(lib, "mfreadwrite.lib")

typedef HRESULT(__stdcall *tMFCreateSinkWriterFromURL)(
	LPCWSTR       pwszOutputURL,
	IMFByteStream *pByteStream,
	IMFAttributes *pAttributes,
	IMFSinkWriter **ppSinkWriter
	);
tMFCreateSinkWriterFromURL sinkwriter_orig_func;

HRESULT __stdcall SinkWriterNew(
	LPCWSTR       pwszOutputURL,
	IMFByteStream *pByteStream,
	IMFAttributes *pAttributes,
	IMFSinkWriter **ppSinkWriter
)
{
	logger->trace("SinkWriterNew: enter");
	auto res = sinkwriter_orig_func(pwszOutputURL, pByteStream, pAttributes, ppSinkWriter);
	logger->trace("SinkWriterNew: exit");
	return res;
}

std::shared_ptr<PLH::IATHook> getHookMFCreateSinkWriterFromURL()
{
	std::shared_ptr<PLH::IATHook> hook(new PLH::IATHook);
	hook->SetupHook("mfreadwrite.dll", "MFCreateSinkWriterFromURL", (BYTE*)&SinkWriterNew);
	if (hook->Hook()) {
		sinkwriter_orig_func = hook->GetOriginal<tMFCreateSinkWriterFromURL>();
		return hook;
	}
	else {
		logger->error("Failed to hook into MFCreateSinkWriterFromURL");
		return nullptr;
	}
}

BOOL APIENTRY DllMain(HMODULE hInstance, DWORD reason, LPVOID lpReserved)
{
	switch (reason)
	{
	case DLL_PROCESS_ATTACH:
		logger = spdlog::basic_logger_st(SCRIPT_NAME, SCRIPT_FOLDER);
		sinkwriter_hook = getHookMFCreateSinkWriterFromURL();
		break;
	case DLL_PROCESS_DETACH:
		sinkwriter_hook->UnHook();
		spdlog::drop(SCRIPT_NAME);
		break;
	}		
	return TRUE;
}