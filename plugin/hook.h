#pragma once

#include <polyhook2/PE/IatHook.hpp>
#include <polyhook2/Virtuals/VTableSwapHook.hpp>

template <typename FuncPtr>
class IatHook : private PLH::IatHook {
public:
	IatHook(
		const std::string& library_name,
		const std::string& func_name,
		FuncPtr new_func
	)
		: orig_func(nullptr)
		, PLH::IatHook(library_name, func_name, reinterpret_cast<char*>(new_func), reinterpret_cast<uint64_t*>(&orig_func), L"")
	{
		if (!hook())
			throw std::runtime_error("failed to hook " + func_name);
	}

	template<typename ... Args>
	auto origFunc(Args && ... args) const {
		if (orig_func == nullptr)
			throw std::runtime_error("original function pointer is null");
		return orig_func(std::forward<Args>(args) ...);
	}

	~IatHook() {
		unHook();
	}
private:
	FuncPtr orig_func;
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

PLH::VFuncMap make_redirect_map()
{
	return PLH::VFuncMap();
};

template<uint16_t I, typename FuncPtr, typename ... VFuncTypes>
PLH::VFuncMap make_redirect_map(VFunc<I, FuncPtr> vfunc, VFuncTypes ... vfuncs)
{
	auto map{ make_redirect_map(vfuncs ...) };
	map[I] = reinterpret_cast<uint64_t>(vfunc.func);
	return map;
};

class VTableSwapHook {
public:
	VTableSwapHook(void* pInstance, const PLH::VFuncMap& redirect_map)
		: m_orig_map()
		, m_table(reinterpret_cast<uint64_t>(pInstance), redirect_map, &m_orig_map)
	{
		if (!m_table.hook())
			throw std::runtime_error("vtable swap hook failed");
	};

	template<typename VFuncType, typename ... Args>
	auto orig_func(Args&& ... args) {
		auto func = reinterpret_cast<typename VFuncType::func_type>(m_orig_map.at(VFuncType::func_index));
		return func(std::forward<Args>(args) ...);
	};

	virtual ~VTableSwapHook()
	{
		m_table.unHook();
	}

private:
	PLH::VFuncMap m_orig_map;
	PLH::VTableSwapHook m_table;
};
