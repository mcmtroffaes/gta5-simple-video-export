#include "..\PolyHook\PolyHook\PolyHook.hpp"
#include "..\spdlog\include\spdlog\spdlog.h"
#include <mfplay.h>
#include <mfreadwrite.h>

#pragma comment(lib, "mfreadwrite.lib")

#define SCRIPT_NAME "simple-video-export"
#define SCRIPT_FOLDER "SVE"

std::shared_ptr<spdlog::logger> logger = nullptr;
std::shared_ptr<PLH::IATHook> sinkwriter_hook = nullptr;

#define LOG_INFO(msg) logger->info("{}: {}", __func__, msg)
#define LOG_DEBUG(msg) logger->debug("{}: {}", __func__, msg)
#define LOG_ERROR(msg) logger->error("{}: {}", __func__, msg)
#define LOG_TRACE(msg) logger->trace("{}: {}", __func__, msg)
#define LOG_ENTER LOG_TRACE("enter")
#define LOG_EXIT LOG_TRACE("exit")

std::string ws2s(const std::wstring & s)
{
	int len;
	size_t slength = s.length() + 1;
	len = WideCharToMultiByte(CP_ACP, 0, s.c_str(), slength, 0, 0, 0, 0);
	char* buf = new char[len];
	WideCharToMultiByte(CP_ACP, 0, s.c_str(), slength, buf, len, 0, 0);
	std::string r(buf);
	delete[] buf;
	return r;
}

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
	LOG_ENTER;
	LOG_DEBUG("pwszOutputURL = " + ws2s(pwszOutputURL));
	auto res = sinkwriter_orig_func(pwszOutputURL, pByteStream, pAttributes, ppSinkWriter);
	LOG_EXIT;
	return res;
}

std::shared_ptr<PLH::IATHook> getHookMFCreateSinkWriterFromURL()
{
	LOG_ENTER;
	std::shared_ptr<PLH::IATHook> hook(new PLH::IATHook);
	hook->SetupHook("mfreadwrite.dll", "MFCreateSinkWriterFromURL", (BYTE*)&SinkWriterNew);
	if (hook->Hook()) {
		sinkwriter_orig_func = hook->GetOriginal<tMFCreateSinkWriterFromURL>();
		LOG_DEBUG("MFCreateSinkWriterFromURL hooked");
		LOG_EXIT;
		return hook;
	}
	else {
		LOG_ERROR("MFCreateSinkWriterFromURL hook failed");
		LOG_EXIT;
		return nullptr;
	}
}

BOOL APIENTRY DllMain(HMODULE hInstance, DWORD reason, LPVOID lpReserved)
{
	switch (reason)
	{
	case DLL_PROCESS_ATTACH:
		/* reset all variables */
		logger = nullptr;
		sinkwriter_hook = nullptr;
		/* set up logger */
		logger = spdlog::basic_logger_st(SCRIPT_NAME, SCRIPT_FOLDER "\\log.txt");
		if (!logger) break;
		logger->set_level(spdlog::level::trace);
		logger->flush_on(spdlog::level::trace);
		LOG_ENTER;
		/* set up hooks */
		sinkwriter_hook = getHookMFCreateSinkWriterFromURL();
		break;
	case DLL_PROCESS_DETACH:
		/* clean up hooks */
		if (sinkwriter_hook) {
			sinkwriter_hook->UnHook();
			sinkwriter_hook = nullptr;
			LOG_DEBUG("MFCreateSinkWriterFromURL unhooked");
		}
		/* clean up logger */
		if (logger) {
			LOG_EXIT;
			logger = nullptr;
			spdlog::drop(SCRIPT_NAME);
		}
		break;
	}		
	return TRUE;
}