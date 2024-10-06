#pragma once

#include <d3d11.h>
#include <wrl.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <directxmath.h>

#include "Basic.hpp"

struct GLFWwindow;

class Renderer {
public:
	Renderer(GLFWwindow* window);
	virtual ~Renderer();

	void Render();

private:
	template<typename T>
	using ComPtr = Microsoft::WRL::ComPtr<T>;
	
	ComPtr<IDXGIFactory2> m_factory;
	std::vector<ComPtr<IDXGIAdapter>> m_adapters;
	ComPtr<IDXGISwapChain> m_swapchain;
	//ID3D11Debug

	ComPtr<ID3D11Device> m_device;
	ComPtr<ID3D11DeviceContext> m_deviceContext;

	// apparently we dont need to keep ref of the render target because internally it is referenced? or smth?
	ComPtr<ID3D11RenderTargetView> m_renderTargetView;
	ComPtr<ID3D11Texture2D> m_depthStencil;
	ComPtr<ID3D11DepthStencilView> m_depthStencilView;

	// pipeline states
	ComPtr<ID3D11RasterizerState> m_rasterState;
	ComPtr<ID3D11DepthStencilState> m_depthStencilState;

	D3D11_VIEWPORT m_viewport;

	DirectX::XMMATRIX m_modelToWorld = {};
	DirectX::XMMATRIX m_worldToView = {};
	DirectX::XMMATRIX m_viewToProjection = {};

	ComPtr<ID3D11Buffer> m_vertexBuffer;
	ComPtr<ID3D11Buffer> m_indexBuffer;

	struct SimpleVertexCombined
	{
		DirectX::XMFLOAT3 Pos;
		DirectX::XMFLOAT3 Col;
	};

	GLFWwindow* m_window;
};

//template<>
//struct fmt::formatter<LUID> : fmt::formatter<std::string>
//{
//	auto format(LUID luid, format_context &ctx) const -> decltype(ctx.out())
//	{
//		return format_to(ctx.out(), "[LUID luid={}{}]", luid.HighPart, luid.LowPart);
//	}
//};
//
//template<>
//struct fmt::formatter<DXGI_ADAPTER_DESC1> : fmt::formatter<std::string>
//{
//	auto format(DXGI_ADAPTER_DESC1 desc, buffer_context<char>& ctx) const -> decltype(ctx.out())
//	{
//		char description[128];
//		std::wcstombs(description, desc.Description, 128);
//
//		return format_to(
//			ctx.out(),
//			"[DXGI_ADAPTER_DESC1 Description={} VendorId={} DeviceId={} SubSysId={} Revision={} DedicatedVideoMemory={} DedicatedSystemMemory={} SharedSystemMemory={} AdapterLuid={} Flags={}]",
//			description,
//			desc.VendorId,
//			desc.DeviceId,
//			desc.SubSysId,
//			desc.Revision,
//			desc.DedicatedVideoMemory,
//			desc.DedicatedSystemMemory,
//			desc.SharedSystemMemory,
//			desc.AdapterLuid,
//			desc.Flags
//		);
//	}
//};
