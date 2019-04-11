#include <iostream>
#include "spdlog\sinks\stdout_color_sinks.h"
#include "settings.h"

#pragma comment(lib, "common.lib")

std::shared_ptr<spdlog::logger> logger = nullptr;
std::unique_ptr<Settings> settings = nullptr;

int main()
{
	/* load settings without logger, to get log_level and log_flush_on */
	logger = nullptr;
	settings.reset(new Settings);
	/* set up logger */
	logger = spdlog::stdout_color_mt(SCRIPT_NAME);
	settings->ResetLogger();
	settings.reset(new Settings);
	std::cout << "Hello World!\n";
	std::cin.get();
}
