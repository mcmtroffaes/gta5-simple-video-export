/*
Main entry point for attaching and detaching the mod.

Attach:

1. Get all settings from the ini file.
2. Set up the logger with corresponding settings.
3. Hook MFCreateSinkWriterFromURL.

Detach:

1. Unhook MFCreateSinkWriterFromURL.
2. Clean up settings
3. Clean up logger.
*/

#include "sinkwriter.h"
#include "logger.h"
#include "settings.h"

#pragma comment(lib, "common.lib")

std::shared_ptr<spdlog::logger> logger = nullptr;
std::unique_ptr<Settings> settings = nullptr;

BOOL APIENTRY DllMain(HMODULE hInstance, DWORD reason, LPVOID lpReserved)
{
	switch (reason)
	{
	case DLL_PROCESS_ATTACH:
		/* set up logger */
		logger = spdlog::rotating_logger_mt(
			SCRIPT_NAME, SCRIPT_NAME ".log", 10000000, 5);
		logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%t] [%^%l%$] %v");
		AVLogSetCallback();
		/* load settings */
		settings = std::make_unique<Settings>();
		LOG_ENTER;
		/* set up hooks */
		Hook();
		LOG->info(SCRIPT_NAME " started");
		break;
	case DLL_PROCESS_DETACH:
		/* clean up hooks */
		Unhook();
		/* clean up settings */
		settings = nullptr;
		/* clean up logger */
		LOG->info(SCRIPT_NAME " stopped");
		LOG_EXIT;
		logger = nullptr;
		spdlog::drop(SCRIPT_NAME);
		break;
	}		
	return TRUE;
}
