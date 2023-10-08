#pragma once

#include <unknwn.h>
#include <wrl/client.h>
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

class VTableSwapHook {
public:
	VTableSwapHook(IUnknown* instance, const PLH::VFuncMap& redirect_map)
		: m_com_ptr(instance)
		, m_orig_map()
		, m_table(reinterpret_cast<uint64_t>(instance), redirect_map, &m_orig_map)
	{
		m_com_ptr->AddRef();
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
		if (m_com_ptr)
			m_com_ptr->Release();
	}

private:
	PLH::VFuncMap m_orig_map;
	PLH::VTableSwapHook m_table;
	// com object pointer to get and release ownership
	IUnknown* m_com_ptr;
};
