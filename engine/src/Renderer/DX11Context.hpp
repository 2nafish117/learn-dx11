#pragma once

#include <d3d11.h>
#include <wrl.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <d3dcommon.h>
#include <directxmath.h>
#include <stb/stb_image.h>

#include "Basic.hpp"
#include "Math.hpp"
#include "DX11ContextUtils.hpp"

struct GLFWwindow;

class ShaderAsset;
class MeshAsset;
class Camera;

class Mesh;
class VertexShader;
class PixelShader;

class ShaderCompiler;

class DX11Context {
	template<typename T>
	using ComPtr = Microsoft::WRL::ComPtr<T>;
	
public:
	// @TODO: could be array of windows
	DX11Context(GLFWwindow* window);
	virtual ~DX11Context();

	void Render();

	void Render(std::shared_ptr<Mesh> mesh, std::shared_ptr<VertexShader> shader);

	void HandleResize(u32 width, u32 height);

	ComPtr<ID3D11Device> GetDevice() {
		return m_device;
	}

	ComPtr<ID3D11DeviceContext> GetDeviceContext() {
		return m_deviceContext;
	}


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

	// @TODO: run this at the end of every frame or pass or each dx11 call?
	// probably call on every dx11 draw call? so state can be verified?
	void LogDebugInfo();

	// @TODO: should these be in the utilities?
	void VerifyGraphicsPipeline();
	void VerifyComputePipeline();

private:
	ComPtr<IDXGIFactory2> m_factory;

	ComPtr<IDXGIAdapter> m_selectedAdapter;
	ComPtr<IDXGIOutput> m_selectedOutput;
	ComPtr<IDXGISwapChain1> m_swapchain;

	// @TODO: use the debug interface
#if _DEBUG
	ComPtr<ID3D11Debug> m_debug;
	ComPtr<ID3D11InfoQueue> m_debugInfoQueue;
#endif

	ComPtr<ID3D11Device> m_device;
	ComPtr<ID3D11DeviceContext> m_deviceContext;

	ComPtr<ID3D11RenderTargetView> m_renderTargetView;
	ComPtr<ID3D11Texture2D> m_depthStencilTexture;
	ComPtr<ID3D11DepthStencilView> m_depthStencilView;

	// pipeline states
	ComPtr<ID3D11RasterizerState> m_rasterState;
	ComPtr<ID3D11DepthStencilState> m_depthStencilState;

	D3D11_VIEWPORT m_viewport = {};

	std::shared_ptr<MeshAsset> m_quadMeshAsset;
	std::shared_ptr<Mesh> m_quadMesh; 

	std::shared_ptr<ShaderAsset> m_simpleVertexAsset;
	std::shared_ptr<ShaderAsset> m_simplePixelAsset;

	std::shared_ptr<VertexShader> m_simpleVertex;
	std::shared_ptr<PixelShader> m_simplePixel;

	std::unique_ptr<ShaderCompiler> m_shaderCompiler;

	ComPtr<ID3D11ShaderResourceView> m_testSRV;
	ComPtr<ID3D11SamplerState> m_testSamplerState;

	ComPtr<ID3D11Buffer> m_matrixBuffer;

	ComPtr<ID3D11Buffer> m_pointLightBuffer;

	std::shared_ptr<Camera> m_camera;

	struct MatrixBuffer {
		mat4 ModelToWorld;
		mat4 WorldToView;
		mat4 ViewToProjection;
	};

	struct PointLightBuffer {
		DirectX::XMFLOAT3 Pos;
		DirectX::XMFLOAT3 Col;

		byte _padding[8];
	};

	GLFWwindow* m_window = nullptr;

	// @TODO: use proper allocators
	byte* m_scratchMemory = nullptr;
	u32 m_scratchSize = 0;
};
