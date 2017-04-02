#include "..\PolyHook\PolyHook\PolyHook.hpp"
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
	std::shared_ptr<PLH::IATHook> hook(new PLH::IATHook);
	hook->SetupHook("mfreadwrite.dll", "MFCreateSinkWriterFromURL", (BYTE*)&SinkWriterNew);
	if (hook->Hook()) {
		sinkwriter_orig_func = hook->GetOriginal<tMFCreateSinkWriterFromURL>();
		return hook;
	}
	else {
		return nullptr;
	}
}

int main() {
	std::shared_ptr<PLH::IATHook> sinkwriter_hook = getHookMFCreateSinkWriterFromURL();
	return 0;
}