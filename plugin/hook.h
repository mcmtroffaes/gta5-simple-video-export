#pragma once

#include <polyhook2/PE/IatHook.hpp>
#include <polyhook2/Virtuals/VTableSwapHook.hpp>

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
		if (!hook.hook()) {
			throw std::runtime_error("failed to hook " + func_name);
		}
	}

	template<typename ... Args>
	auto origFunc(Args && ... args) const {
		if (orig_func == nullptr)
			throw std::runtime_error("original function pointer is null");
		return orig_func(std::forward<Args>(args) ...);
	}

	~IatHook() {
		hook.unHook();
	}
private:
	std::string func_name;
	FuncPtr orig_func;
	PLH::IatHook hook;
};

// storage class for address of a virtual function
// also stores the function pointer type and index number on the class level
template<uint16_t I, typename FuncPtr>
struct VFunc {
	VFunc() : func(nullptr) {};
	VFunc(FuncPtr f) : func(f) {};
	const FuncPtr func;
	static const uint16_t func_index;
	typedef FuncPtr func_type;
};

// definition of constant must reside outside class declaration
template<uint16_t I, typename FuncPtr> const uint16_t VFunc<I, FuncPtr>::func_index = I;

namespace detail {

	// helper function to convert sequence of VFunc structs into a VFuncMap
	// using recursive template definition
	PLH::VFuncMap make_vfunc_map()
	{
		return PLH::VFuncMap{ };
	};

	template<uint16_t I, typename FuncPtr, typename ... VFuncTypes>
	PLH::VFuncMap make_vfunc_map(VFunc<I, FuncPtr> vfunc, VFuncTypes ... vfuncs)
	{
		PLH::VFuncMap map{ {I, reinterpret_cast<uint64_t>(vfunc.func)} };
		map.merge(make_vfunc_map(vfuncs ...));
		return map;
	};

}

template<class ClassType, typename ... Ts>
class VTableSwapHook {
public:
	VTableSwapHook(ClassType& instance_, Ts ... new_funcs_)
		: hook((char *)&instance_, detail::make_vfunc_map(new_funcs_ ...))
	{
		if (!hook.hook())
			throw std::runtime_error(std::string("failed to hook ") + typeid(ClassType).name());
	};

	template<typename VFuncType, typename ... Args>
	auto origFunc(Args && ... args) {
		auto func = reinterpret_cast<typename VFuncType::func_type>(hook.getOriginals().at(VFuncType::func_index));
		if (func == nullptr)
			throw std::runtime_error("original virtual function pointer is null");
		return func(std::forward<Args>(args) ...);
	};

	~VTableSwapHook()
	{
		hook.unHook();
	}
private:
	PLH::VTableSwapHook hook;
};
