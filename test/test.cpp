#include <iostream>
#include "..\spdlog\include\spdlog\sinks\stdout_sinks.h"
#include "..\gta5-simple-video-export\settings.h"

std::shared_ptr<spdlog::logger> logger = nullptr;
std::unique_ptr<Settings> settings = nullptr;

int main()
{
	/* load settings without logger, to get log_level and log_flush_on */
	logger = spdlog::rotating_logger_mt;
	settings.reset(new Settings);
	/* set up logger */
	settings->ResetLogger();
	std::cout << "Hello World!\n";
	std::cin.get();
}
