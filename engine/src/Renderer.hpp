#pragma once

#include <d3d11.h>
#include <wrl.h>
#include <dxgi.h>
#include <d3dcompiler.h>
#include <directxmath.h>

#include "Basic.hpp"

class Renderer {
public:
	Renderer();
	virtual ~Renderer();

private:
	template<typename T>
	using ComPtr = Microsoft::WRL::ComPtr<T>;
	
	ComPtr<IDXGIFactory1> m_factory;

	ComPtr<ID3D11Device> m_device;
	ComPtr<ID3D11DeviceContext> m_deviceContext;
};

 template<>
 struct fmt::formatter<LUID> : fmt::formatter<std::string>
 {
     auto format(LUID luid, format_context &ctx) const -> decltype(ctx.out())
     {
         return format_to(ctx.out(), "[LUID luid={}_{}]", luid.HighPart, luid.LowPart);
     }
 };

template<>
struct fmt::formatter<DXGI_ADAPTER_DESC1> : fmt::formatter<std::string>
{
	auto format(DXGI_ADAPTER_DESC1 desc, buffer_context<char>& ctx) const -> decltype(ctx.out())
	{
		char description[128];
		std::wcstombs(description, desc.Description, 128);

		return format_to(
			ctx.out(),
			"[DXGI_ADAPTER_DESC1 Description={} VendorId={} DeviceId={} SubSysId={} Revision={} DedicatedVideoMemory={} DedicatedSystemMemory={} SharedSystemMemory={} AdapterLuid={} Flags={}]",
			description,
			desc.VendorId,
			desc.DeviceId,
			desc.SubSysId,
			desc.Revision,
			desc.DedicatedVideoMemory,
			desc.DedicatedSystemMemory,
			desc.SharedSystemMemory,
			desc.AdapterLuid,
			desc.Flags
		);
	}
};
