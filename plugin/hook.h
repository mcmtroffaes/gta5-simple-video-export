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

template<typename ... VFuncTypes>
class VTableSwapHook : private PLH::VTableSwapHook {
public:
	VTableSwapHook(IUnknown* instance, VFuncTypes ... new_funcs)
		: PLH::VTableSwapHook(reinterpret_cast<uint64_t>(instance), new_funcs ...)
		, m_com_ptr(instance)
		, m_shared_ptr(nullptr)
	{
		m_com_ptr->AddRef();
		if (!hook())
			throw std::runtime_error("vtable swap hook failed");
	};

	template <typename T>
	VTableSwapHook(std::shared_ptr<T> instance, VFuncTypes ... new_funcs)
		: PLH::VTableSwapHook(reinterpret_cast<uint64_t>(instance.get()), new_funcs ...)
		, m_com_ptr(nullptr)
		, m_shared_ptr(instance)
	{
		if (!hook())
			throw std::runtime_error("vtable swap hook failed");
	};

	template<typename VFuncType, typename ... Args>
	inline auto origFunc(Args && ... args) {
		static_assert(std::disjunction_v<std::is_same<VFuncType, VFuncTypes> ...>); 
		return PLH::VTableSwapHook::origFunc<VFuncType>(std::forward<Args>(args) ...);
	};

	virtual ~VTableSwapHook()
	{
		unHook();
		if (m_com_ptr)
			m_com_ptr->Release();
	}

private:
	// com object pointer to get and release ownership
	IUnknown* m_com_ptr;
	// shared pointer to maintain ownership
	std::shared_ptr<void> m_shared_ptr;
};
