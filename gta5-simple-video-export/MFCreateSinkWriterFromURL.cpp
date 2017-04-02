#include "..\PolyHook\PolyHook\PolyHook.hpp"
#include "..\spdlog\include\spdlog\spdlog.h"
#include <mfplay.h>
#include <mfreadwrite.h>

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
	return sinkwriter_orig_func(pwszOutputURL, pByteStream, pAttributes, ppSinkWriter);
}

std::shared_ptr<PLH::IATHook> getHookMFCreateSinkWriterFromURL()
{
	auto console = spdlog::stdout_color_mt("console");
	std::shared_ptr<PLH::IATHook> hook(new PLH::IATHook);
	hook->SetupHook("mfreadwrite.dll", "MFCreateSinkWriterFromURL", (BYTE*)&SinkWriterNew);
	if (hook->Hook()) {
		sinkwriter_orig_func = hook->GetOriginal<tMFCreateSinkWriterFromURL>();
		return hook;
	}
	else {
		console->error("Failed to hook into MFCreateSinkWriterFromURL");
		return nullptr;
	}
}

int main() {
	std::shared_ptr<PLH::IATHook> sinkwriter_hook = getHookMFCreateSinkWriterFromURL();
	getchar();
	return 0;
}