#include "sinkwriter.h"
#include "logger.h"

#define SCRIPT_NAME "simple-video-export"
#define SCRIPT_FOLDER "SVE"

std::shared_ptr<spdlog::logger> logger = nullptr;

BOOL APIENTRY DllMain(HMODULE hInstance, DWORD reason, LPVOID lpReserved)
{
	switch (reason)
	{
	case DLL_PROCESS_ATTACH:
		/* set up logger */
		logger = spdlog::basic_logger_st(SCRIPT_NAME, SCRIPT_FOLDER "\\log.txt");
		if (!logger) break;
		logger->set_level(spdlog::level::trace);
		logger->flush_on(spdlog::level::trace);
		LOG_ENTER;
		/* set up hooks */
		Hook();
		break;
	case DLL_PROCESS_DETACH:
		/* clean up hooks */
		Unhook();
		if (logger) LOG_EXIT;
		/* clean up logger */
		logger = nullptr;
		spdlog::drop(SCRIPT_NAME);
		break;
	}		
	return TRUE;
}
