#pragma once

// windows defines min and max as macros, disable with this, wtf???!! why!!??
#define NOMINMAX
#include <d3d11_1.h>
#include <wrl.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <d3dcommon.h>
#include <directxmath.h>

#include "Basic.hpp"
#include "Math.hpp"
#include <comdef.h>

struct GLFWwindow;

#ifdef _DEBUG
#define DX11_DEBUG
#endif

static void dxlog(spdlog::level::level_enum lvl, const char* file, int line, HRESULT hr) {
	_com_error err(hr);
	spdlog::log(lvl, "[DX] {}:{} : {} ({:x})", file, line, err.ErrorMessage(), (u32)hr);
}

// @TODO: use the levels correctly
#define DXCRIT(hr)	dxlog(spdlog::level::critical, __FILE__, __LINE__, hr)
#define DXERROR(hr) dxlog(spdlog::level::err, __FILE__, __LINE__, hr)
#define DXWARN(hr)	dxlog(spdlog::level::warn, __FILE__, __LINE__, hr)

// @TODO: rename class?
// @TODO: support immediate context creation

class DX11Context {
	template<typename T>
	using ComPtr = Microsoft::WRL::ComPtr<T>;
	
public:
	// @TODO: could be array of windows
	DX11Context(GLFWwindow* window);
	virtual ~DX11Context();

	inline ComPtr<ID3D11Device> GetDevice() {
		return m_device;
	}

	inline ComPtr<ID3D11DeviceContext> GetDeviceContext() {
		return m_deviceContext;
	}
	
	inline ComPtr<IDXGISwapChain1> GetSwapchain() {
		return m_swapchain;
	}

	inline ComPtr<ID3DUserDefinedAnnotation> GetAnnotation() {
		return m_annotation;
	}

	void EnumAdapters(std::vector<ComPtr<IDXGIAdapter>>& outAdapters);
	ComPtr<IDXGIAdapter> PickAdapter(const std::vector<ComPtr<IDXGIAdapter>>& adapters);
	void EnumOutputs(ComPtr<IDXGIAdapter> adapter, std::vector<ComPtr<IDXGIOutput>>& outOutputs);
	ComPtr<IDXGIOutput> PickOutput(const std::vector<ComPtr<IDXGIOutput>>& outputs);

	void CreateDeviceAndContext(UINT createFlags);

	void CreateSwapchain(GLFWwindow* window, u32 bufferCount);

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
#ifdef DX11_DEBUG
	ComPtr<ID3D11Debug> m_debug;
	ComPtr<ID3D11InfoQueue> m_debugInfoQueue;
#endif
	ComPtr<ID3DUserDefinedAnnotation> m_annotation;

	ComPtr<ID3D11Device> m_device;
	ComPtr<ID3D11DeviceContext> m_deviceContext;

	GLFWwindow* m_window = nullptr;

	// @TODO: use proper allocators
	byte* m_scratchMemory = nullptr;
	u32 m_scratchSize = 0;
};
