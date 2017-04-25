#pragma once

#include "logger.h"
#include "../PolyHook/PolyHook/PolyHook.hpp"

template <class FuncType>
std::unique_ptr<PLH::IATHook> CreateIATHook(const char *library_name, const char *func_name, FuncType new_func) {
	LOG_ENTER;
	std::unique_ptr<PLH::IATHook> hook(new PLH::IATHook());
	hook->SetupHook(library_name, func_name, (BYTE*)new_func);
	if (hook->Hook()) {
		logger->debug("hook set up for {}", func_name);
	}
	else {
		hook = nullptr;
		logger->error("failed to set up hook for {} ({})", func_name, hook->GetLastError().GetString());
	}
	LOG_EXIT;
	return hook;
}

template <class ClassType, class FuncType>
std::unique_ptr<PLH::VFuncDetour> CreateVFuncDetour(ClassType *p_instance, int vfunc_index, FuncType new_func) {
	LOG_ENTER;
	std::unique_ptr<PLH::VFuncDetour> hook(new PLH::VFuncDetour());
	if (!p_instance) {
		hook = nullptr;
		logger->error("failed to set up hook: pointer to class instance is nullptr");
	}
	else {
		hook->SetupHook(*(BYTE***)p_instance, vfunc_index, (BYTE*)new_func);
		if (hook->Hook()) {
			logger->debug("hook set up");
		}
		else {
			hook = nullptr;
			logger->error("failed to set up hook ({})", hook->GetLastError().GetString());
		}
	}
	LOG_EXIT;
	return hook;
}
