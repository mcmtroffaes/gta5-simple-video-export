/* helper functions for creating hooks */

#pragma once

#include "logger.h"
#include "../PolyHook/PolyHook/PolyHook.hpp"

template <class FuncType>
std::unique_ptr<PLH::IATHook> CreateIATHook(const char *library_name, const char *func_name, FuncType new_func) {
	LOG_ENTER;
	std::unique_ptr<PLH::IATHook> hook(new PLH::IATHook());
	hook->SetupHook(library_name, func_name, (BYTE*)new_func);
	if (hook->Hook()) {
		logger->debug("hook set for {}", func_name);
	}
	else {
		hook = nullptr;
		logger->debug("failed to hook {}", func_name);
	}
	return hook;
	LOG_EXIT;
}

/* helper function to create a hook for a virtual function */
template <class ClassType, class FuncType>
std::unique_ptr<PLH::VFuncDetour> CreateVFuncDetour(ClassType *p_instance, int vfunc_index, FuncType new_func) {
	LOG_ENTER;
	std::unique_ptr<PLH::VFuncDetour> hook(new PLH::VFuncDetour());
	if (!p_instance) {
		hook = nullptr;
		LOG_ERROR("hook failed: class instance is null");
	}
	else {
		hook->SetupHook(*(BYTE***)p_instance, vfunc_index, (BYTE*)new_func);
		if (hook->Hook()) {
			LOG_DEBUG("hook set");
		}
		else {
			hook = nullptr;
			LOG_ERROR("hook failed");
		}
	}
	LOG_EXIT;
	return hook;
}
