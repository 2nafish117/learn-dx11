#pragma once

#include <d3d11.h>
#include <wrl.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <directxmath.h>
#include <limits>
#include <stb/stb_image.h>

#include "Basic.hpp"

struct GLFWwindow;

class Renderer {
	template<typename T>
	using ComPtr = Microsoft::WRL::ComPtr<T>;
	
public:
	// @TODO: could be array of windows
	Renderer(GLFWwindow* window);
	virtual ~Renderer();

	void Render();
	void HandleResize(u32 width, u32 height);

private:
	void EnumAdapters(std::vector<ComPtr<IDXGIAdapter>>& outAdapters);
	ComPtr<IDXGIAdapter> PickAdapter(const std::vector<ComPtr<IDXGIAdapter>>& adapters);
	void EnumOutputs(ComPtr<IDXGIAdapter> adapter, std::vector<ComPtr<IDXGIOutput>>& outOutputs);
	ComPtr<IDXGIOutput> PickOutput(const std::vector<ComPtr<IDXGIOutput>>& outputs);

	void CreateDeviceAndContext(UINT createFlags);

	void CreateSwapchain(GLFWwindow* window, u32 bufferCount);

	// @TODO: factor swapchain params?
	void ResizeSwapchainResources(u32 width, u32 height);
	void ObtainSwapchainResources();
	void ReleaseSwapchainResources();

	ComPtr<IDXGIFactory2> m_factory;

	ComPtr<IDXGIAdapter> m_selectedAdapter;
	ComPtr<IDXGIOutput> m_selectedOutput;
	ComPtr<IDXGISwapChain1> m_swapchain;

	// @TODO: use the debug interface
	ComPtr<ID3D11Debug> m_debug;
	ComPtr<ID3D11Device> m_device;
	ComPtr<ID3D11DeviceContext> m_deviceContext;

	ComPtr<ID3D11RenderTargetView> m_renderTargetView;
	ComPtr<ID3D11Texture2D> m_depthStencilTexture;
	ComPtr<ID3D11DepthStencilView> m_depthStencilView;

	// pipeline states
	ComPtr<ID3D11RasterizerState> m_rasterState;
	ComPtr<ID3D11DepthStencilState> m_depthStencilState;

	D3D11_VIEWPORT m_viewport = {};

	// @TODO move these to a model class
	ComPtr<ID3D11Buffer> m_vertexBuffer;
	ComPtr<ID3D11Buffer> m_indexBuffer;

	ComPtr<ID3D11VertexShader> m_simpleVertex;
	ComPtr<ID3D11PixelShader> m_simplePixel;

	ComPtr<ID3D11ShaderResourceView> m_testSRV;
	ComPtr<ID3D11SamplerState> m_testSamplerState;

	ComPtr<ID3D11Buffer> m_matrixBuffer;

	ComPtr<ID3D11Buffer> m_pointLightBuffer;

	struct SimpleVertexCombined
	{
		DirectX::XMFLOAT3 Pos;
		DirectX::XMFLOAT3 Normal;
		DirectX::XMFLOAT3 Col;
		DirectX::XMFLOAT2 UV0;
	};

	struct MatrixBuffer {
		DirectX::XMMATRIX ModelToWorld;
		DirectX::XMMATRIX WorldToView;
		DirectX::XMMATRIX ViewToProjection;
	};

	struct PointLightBuffer {
		DirectX::XMFLOAT3 Pos;
		DirectX::XMFLOAT3 Col;

		byte _padding[8];
	};

	GLFWwindow* m_window = nullptr;
};
