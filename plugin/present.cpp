#include "present.h"
#include "logger.h"

#include <d3d11.h>
#pragma comment(lib, "d3d11.lib")

#include "../sdk/inc/main.h"
#pragma comment(lib, "../sdk/lib/scripthookv.lib")

void SwapChainPresentCallback(void* pVoid)
{
	LOG_ENTER;
	auto pSwapChain = reinterpret_cast<IDXGISwapChain*>(pVoid);
	LOG_EXIT;
}

SwapChainPresentHook::SwapChainPresentHook()
{
	presentCallbackRegister(SwapChainPresentCallback);
};

SwapChainPresentHook::~SwapChainPresentHook()
{
	presentCallbackUnregister(SwapChainPresentCallback);
};