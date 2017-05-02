#include "sinkwriter.h"
#include "logger.h"
#include "settings.h"

std::shared_ptr<spdlog::logger> logger = nullptr;
std::unique_ptr<Settings> settings = nullptr;

BOOL APIENTRY DllMain(HMODULE hInstance, DWORD reason, LPVOID lpReserved)
{
	switch (reason)
	{
	case DLL_PROCESS_ATTACH:
		/* set up logger */
		logger = spdlog::basic_logger_st(SCRIPT_NAME, SCRIPT_FOLDER "\\log.txt");
		if (!logger) break;
		logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [thread %t] [%l] %v");
		/* load settings */
		settings.reset(new Settings());
		settings->Load();
		logger->set_level(settings->log_level_);
		logger->flush_on(settings->log_flush_on_);
		LOG_ENTER;
		/* set up hooks */
		Hook();
		break;
	case DLL_PROCESS_DETACH:
		/* clean up hooks */
		Unhook();
		/* clean up settings */
		settings = nullptr;
		/* clean up logger */
		if (logger) LOG_EXIT;
		logger = nullptr;
		spdlog::drop(SCRIPT_NAME);
		break;
	}		
	return TRUE;
}
