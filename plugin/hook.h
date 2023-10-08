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

class VTableSwapHook : private PLH::VTableSwapHook {
public:
	VTableSwapHook(IUnknown* instance, const PLH::VFuncMap& redirectMap, PLH::VFuncMap& origVFuncs)
		: PLH::VTableSwapHook(reinterpret_cast<uint64_t>(instance), redirectMap, &origVFuncs)
		, m_com_ptr(instance)
	{
		m_com_ptr->AddRef();
		if (!hook())
			throw std::runtime_error("vtable swap hook failed");
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
};
