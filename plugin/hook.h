#pragma once

#include "logger.h"
#include <polyhook2/ErrorLog.hpp>
#include <polyhook2/PE/IatHook.hpp>
#include <polyhook2/Virtuals/VTableSwapHook.hpp>

#include <tuple>

template <typename FuncPtr>
class IatHook {
public:
	IatHook(
		const std::string& library_name_,
		const std::string& func_name_,
		FuncPtr new_func_ptr_
	)
		: func_name(func_name_)
		, orig_func(nullptr)
		, hook(library_name_, func_name_, reinterpret_cast<char*>(new_func_ptr_), reinterpret_cast<uint64_t*>(&orig_func), L"")
	{
		LOG_ENTER_METHOD;
		logger->debug("hooking {}", func_name);
		auto hooked = hook.hook();
		PLHLog();
		if (!hooked) {
			LOG_EXIT_METHOD;
			throw std::runtime_error(fmt::format("failed to hook {}", func_name));
		}
		logger->trace("redirected {:#x} to {:#x}", reinterpret_cast<uint64_t>(orig_func), reinterpret_cast<uint64_t>(new_func_ptr_));
		LOG_EXIT_METHOD;
	}
	template<typename... Args>
	auto origFunc(Args&&... args) const {
		LOG_ENTER_METHOD;
		if (orig_func == nullptr)
			throw std::runtime_error("original function pointer is null");
		return orig_func(std::forward<Args>(args)...);
		LOG_EXIT_METHOD;
	}

	~IatHook() {
		LOG_ENTER_METHOD;
		logger->debug("unhooking {}", func_name);
		hook.unHook();
		PLHLog();
		LOG_EXIT_METHOD;
	}
private:
	std::string func_name;
	FuncPtr orig_func;
	PLH::IatHook hook;
};

template<uint16_t I, typename FuncPtr>
struct VFunc {
	VFunc() : func(nullptr) {};
	VFunc(FuncPtr f) : func(f) {};
	FuncPtr func;
};

template<uint16_t I, typename FuncPtr>
VFunc<I, FuncPtr> make_vfunc(FuncPtr f)
{
	return VFunc<I, FuncPtr>(f);
}

PLH::VFuncMap make_vfunc_map()
{
	return PLH::VFuncMap{ };
};

template<uint16_t I, typename FuncPtr, typename ... Ts>
PLH::VFuncMap make_vfunc_map(VFunc<I, FuncPtr> vfunc, Ts ... vfuncs)
{
	PLH::VFuncMap map{ {I, reinterpret_cast<uint64_t>(vfunc.func)} };
	map.merge(make_vfunc_map(vfuncs ...));
	return map;
};

std::tuple<> make_vfunc_tuple(const PLH::VFuncMap& map)
{
	return std::make_tuple<>();
}

// dummys are only used for automatic type inference
template<uint16_t I, typename FuncPtr, typename ... Ts>
std::tuple<VFunc<I, FuncPtr>, Ts ...> make_vfunc_tuple(const PLH::VFuncMap& map, const VFunc<I, FuncPtr>& dummy, Ts ... dummys)
{
	auto t1 = std::make_tuple(VFunc<I, FuncPtr>(reinterpret_cast<FuncPtr>(map.at(I))));
	auto t2 = make_vfunc_tuple(map, dummys ...);
	return std::tuple_cat(t1, t2);
}

void log_vfunc_map(const PLH::VFuncMap& map)
{
	for (auto&& [first, second] : map) {
		logger->trace("virtual method {} -> {:#x}", first, second);
	}
}

template<class ClassType, typename ... Ts>
class VTableSwapHook {
public:
	VTableSwapHook(ClassType& instance_, Ts ... new_funcs_)
		: hook((char *)&instance_, make_vfunc_map(new_funcs_ ...))
	{
		LOG_ENTER_METHOD;
		logger->debug("hooking {}", typeid(ClassType).name());
		auto hooked = hook.hook();
		PLHLog();
		if (!hooked) {
			LOG_EXIT_METHOD;
			throw std::runtime_error(fmt::format("failed to hook {}", typeid(ClassType).name()));
		}
		orig_funcs = make_vfunc_tuple(hook.getOriginals(), new_funcs_ ...);
		if (logger->level() <= spdlog::level::trace) {
			logger->trace("original pointers:");
			log_vfunc_map(hook.getOriginals());
			// hook.m_redirectMap is private, so just recreate it
			auto vfunc_map = make_vfunc_map(new_funcs_ ...);
			logger->trace("new pointers:");
			log_vfunc_map(vfunc_map);
		}
		LOG_EXIT_METHOD;
	};

	template<uint16_t N, typename ... Args>
	auto origFunc(Args ... args) {
		auto func = std::get<N>(orig_funcs).func;
		if (func == nullptr)
			throw std::runtime_error("original function pointer is null");
		return func(args ...);
	};

	~VTableSwapHook()
	{
		LOG_ENTER_METHOD;
		logger->debug("unhooking {}", typeid(ClassType).name());
		hook.unHook();
		PLHLog();
		LOG_EXIT_METHOD;
	}
private:
	std::tuple<Ts ...> orig_funcs;
	PLH::VTableSwapHook hook;
};
