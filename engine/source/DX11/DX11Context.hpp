#pragma once

#ifdef _DEBUG
#define DX11_DEBUG
#endif

// windows defines min and max as macros, disable with this, wtf???!! why!!??
#define NOMINMAX
#include <d3d11_1.h>
#include <wrl.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <d3dcommon.h>
#include <directxmath.h>

#ifdef DX11_DEBUG
#include <dxgidebug.h>
#endif

#include "Basic.hpp"
#include "Math.hpp"
#include "DX11ContextUtils.hpp"

#include "AssetSystem.hpp"

struct GLFWwindow;

class DX11Mesh;
class DX11VertexShader;
class DX11PixelShader;
class ShaderCompiler;

class RuntimeScene;

class DX11Context {
	template<typename T>
	using ComPtr = Microsoft::WRL::ComPtr<T>;
	
public:
	// @TODO: could be array of windows
	DX11Context(GLFWwindow* window);
	virtual ~DX11Context();

	void Render(const RuntimeScene& scene);
	void HandleResize(u32 width, u32 height);

	void InitImgui();

	inline ComPtr<ID3D11Device> GetDevice() {
		return m_device;
	}

	inline ComPtr<ID3D11DeviceContext> GetDeviceContext() {
		return m_deviceContext;
	}

public:
	std::unique_ptr<ShaderCompiler> shaderCompiler;

private:
	void EnumAdapters(std::vector<ComPtr<IDXGIAdapter>>& outAdapters);
	ComPtr<IDXGIAdapter> PickAdapter(const std::vector<ComPtr<IDXGIAdapter>>& adapters);
	void EnumOutputs(ComPtr<IDXGIAdapter> adapter, std::vector<ComPtr<IDXGIOutput>>& outOutputs);
	ComPtr<IDXGIOutput> PickOutput(const std::vector<ComPtr<IDXGIOutput>>& outputs);
	void GetOutputModes(std::vector<DXGI_MODE_DESC>& outOutputModes);

	void CreateDeviceAndContext(UINT createFlags);

	void CreateSwapchain(GLFWwindow* window, u32 bufferCount);

	void CreateGbuffer(uint width, uint height);

	// @TODO: factor swapchain params?
	void ResizeSwapchainResources(u32 width, u32 height);
	void ObtainSwapchainResources();
	void ReleaseSwapchainResources();

	// convinience function to log messages from d3d api, by default called at the end of a frame just before present
	// but can be called anytime during Render
	void LogDebugInfo();

	void ReportLiveDeviceObjects();

	// @TODO: should these be in the utilities?
	void VerifyGraphicsPipeline();
	void VerifyComputePipeline();

private:
	ComPtr<IDXGIFactory2> m_factory;

	ComPtr<IDXGIAdapter> m_selectedAdapter;
	ComPtr<IDXGIOutput> m_selectedOutput;
	ComPtr<IDXGISwapChain1> m_swapchain;

	// @TODO: use the debug interface
#ifdef DX11_DEBUG
	ComPtr<ID3D11Debug> m_debug;
	ComPtr<ID3D11InfoQueue> m_debugInfoQueue;
	ComPtr<IDXGIDebug> m_dxgiDebug;
#endif
	ComPtr<ID3DUserDefinedAnnotation> m_annotation;

	ComPtr<ID3D11Device> m_device;
	ComPtr<ID3D11DeviceContext> m_deviceContext;

	ComPtr<ID3D11RenderTargetView> m_renderTargetView;
	ComPtr<ID3D11Texture2D> m_depthStencilTexture;
	ComPtr<ID3D11DepthStencilView> m_depthStencilView;

	// pipeline states
	ComPtr<ID3D11RasterizerState> m_rasterState;
	ComPtr<ID3D11RasterizerState> m_rasterState2;
	ComPtr<ID3D11DepthStencilState> m_depthStencilState;

	struct GBufferData {
		ComPtr<ID3D11Texture2D> albedoTexture;
		ComPtr<ID3D11RenderTargetView> albedoRTV;
		ComPtr<ID3D11ShaderResourceView> albedoSRV;

		ComPtr<ID3D11Texture2D> wsPositionTexture;
		ComPtr<ID3D11RenderTargetView> wsPositionRTV;
		ComPtr<ID3D11ShaderResourceView> wsPositionSRV;

		ComPtr<ID3D11Texture2D> wsNormalTexture;
		ComPtr<ID3D11RenderTargetView> wsNormalRTV;
		ComPtr<ID3D11ShaderResourceView> wsNormalSRV;
	};

	GBufferData m_gbufferData;

	// final pass quad mesh
	// @TODO: hardcoding asset ids
	MeshID m_quadMesh { 0 };
	ShaderID m_finalPassVertexShader { 4 };
	ShaderID m_finalPassPixelShader { 5 };

	D3D11_VIEWPORT m_viewport = {};

	ComPtr<ID3D11Buffer> m_matrixBuffer;
	ComPtr<ID3D11Buffer> m_pointLightBuffer;

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


// @TODO: implement a sampler state cache for all common sampler state types
class SamplerStateCache {
public:

private:
};