#include "Renderer.hpp"

#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include <comdef.h>

static void dxlog(spdlog::level::level_enum lvl, const char* file, int line, HRESULT hr) {
	_com_error err(hr);
	spdlog::log(spdlog::level::err, "[DX] {}:{} : {} ({:x})", file, line, err.ErrorMessage(), (u32)hr);
}

// @TODO: use the levels correctly
#define DXCRIT(hr)	dxlog(spdlog::level::critical, __FILE__, __LINE__, hr)
#define DXERROR(hr) dxlog(spdlog::level::err, __FILE__, __LINE__, hr)
#define DXWARN(hr)	dxlog(spdlog::level::warn, __FILE__, __LINE__, hr)

Renderer::Renderer(GLFWwindow* window)
	: m_window(window)
{
	assert(m_window != nullptr);

	UINT factoryCreateFlags = 0;

#ifdef _DEBUG
	factoryCreateFlags |= D3D11_CREATE_DEVICE_FLAG::D3D11_CREATE_DEVICE_DEBUG;
#endif

	if(auto res = CreateDXGIFactory2(factoryCreateFlags, IID_PPV_ARGS(&m_factory)); FAILED(res)) {
		DXCRIT(res);
		return;
	}

	std::vector<ComPtr<IDXGIAdapter>> adapters;
	adapters.reserve(8);
	EnumAdapters(adapters);

	m_selectedAdapter = PickAdapter(adapters);

	std::vector<ComPtr<IDXGIOutput>> outputs;
	outputs.reserve(8);
	EnumOutputs(m_selectedAdapter, outputs);

	m_selectedOutput = PickOutput(outputs);

	UINT deviceCreateFlags = 0;
#ifdef _DEBUG
	deviceCreateFlags |= D3D11_CREATE_DEVICE_FLAG::D3D11_CREATE_DEVICE_DEBUG;
#endif

	CreateDeviceAndContext(deviceCreateFlags);

#if _DEBUG
	if (auto res = m_device->QueryInterface(IID_PPV_ARGS(&m_debug)); FAILED(res)) {
		DXERROR(res);
	}
#endif

	CreateSwapchain(m_window, 1);

	ComPtr<ID3D11Texture2D> backBuffer;
	if (auto res = m_swapchain->GetBuffer(0, IID_PPV_ARGS(&backBuffer)); FAILED(res)) {
		DXERROR(res);
	}

	spdlog::info("got back buffer from swapchain");

	D3D11_RENDER_TARGET_VIEW_DESC rtDesc = {
		.Format = DXGI_FORMAT_R8G8B8A8_UNORM,
		.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D,
		.Texture2D = {
			.MipSlice = 0,
		}
	};

	if (auto res = m_device->CreateRenderTargetView(backBuffer.Get(), &rtDesc, &m_renderTargetView); FAILED(res)) {
		DXERROR(res);
		return;
	}

	spdlog::info("created render target view");

	int width, height;
	glfwGetWindowSize(m_window, &width, &height);

	D3D11_TEXTURE2D_DESC depthStencilTexDesc = {
		.Width = (UINT)width,
		.Height = (UINT)height,
		.MipLevels = 1,
		.ArraySize = 1,
		.Format = DXGI_FORMAT_D24_UNORM_S8_UINT,
		.SampleDesc = {
			.Count = 1,
			.Quality = 0,
		},
		.Usage = D3D11_USAGE_DEFAULT,
		.BindFlags = D3D11_BIND_DEPTH_STENCIL,
		.CPUAccessFlags = 0,
		.MiscFlags = 0,
	};

	if(auto res = m_device->CreateTexture2D(&depthStencilTexDesc, nullptr, &m_depthStencil); FAILED(res)) {
		_com_error err(res);
		spdlog::critical("depth stencil texture creation failed with {} ({})", err.ErrorMessage(), res);
	}

	spdlog::info("created depth stencil texture");

	D3D11_DEPTH_STENCIL_DESC depthStencilDesc = {
	.DepthEnable = true,
	.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL,
	.DepthFunc = D3D11_COMPARISON_LESS,
	.StencilEnable = false,
	.StencilReadMask = 0,
	.StencilWriteMask = 0,
	.FrontFace = {
		.StencilFailOp = D3D11_STENCIL_OP_KEEP,
		.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP,
		.StencilPassOp = D3D11_STENCIL_OP_KEEP,
		.StencilFunc = D3D11_COMPARISON_EQUAL,
	},
	.BackFace = {
		.StencilFailOp = D3D11_STENCIL_OP_KEEP,
		.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP,
		.StencilPassOp = D3D11_STENCIL_OP_KEEP,
		.StencilFunc = D3D11_COMPARISON_EQUAL,
	},
	};

	if (auto res = m_device->CreateDepthStencilState(&depthStencilDesc, &m_depthStencilState); FAILED(res)) {
		spdlog::critical("depth stencil state creation failed with {}", res);
	}

	spdlog::info("created depth stencil state");

	m_deviceContext->OMSetDepthStencilState(m_depthStencilState.Get(), 0);

	D3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc = {
		.Format = DXGI_FORMAT_D24_UNORM_S8_UINT,
		.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D,
		.Flags = 0,
		.Texture2D = {
			.MipSlice = 0,
		}
	};
	
	if(auto res = m_device->CreateDepthStencilView(m_depthStencil.Get(), &depthStencilViewDesc, &m_depthStencilView); FAILED(res)) {
		spdlog::critical("depth stencil view creation failed with {}", res);
	}

	m_deviceContext->OMSetRenderTargets(1, m_renderTargetView.GetAddressOf(), m_depthStencilView.Get());

	D3D11_RASTERIZER_DESC rasterDesc = {
		.FillMode = D3D11_FILL_SOLID,
		.CullMode = D3D11_CULL_BACK,
		.FrontCounterClockwise = false,
		.DepthBias = 0,
		.DepthBiasClamp = 0.0f,
		.SlopeScaledDepthBias = 0.0f,
		.DepthClipEnable = true,
		.ScissorEnable = false,
		.MultisampleEnable = false,
		.AntialiasedLineEnable = false,
	};

	// Create the rasterizer state from the description we just filled out.
	if(auto res = m_device->CreateRasterizerState(&rasterDesc, &m_rasterState); FAILED(res)) {
		spdlog::critical("create rasterizer state failed with: {}", res);
	}

	m_deviceContext->RSSetState(m_rasterState.Get());

	m_viewport = {
		.TopLeftX = 0.0f,
		.TopLeftY = 0.0f,
		.Width = (float)width,
		.Height = (float)height,
		.MinDepth = 0.0f,
		.MaxDepth = 1.0f,
	};

	m_deviceContext->RSSetViewports(1, &m_viewport);

	m_modelToWorld = DirectX::XMMatrixIdentity();
	m_worldToView = DirectX::XMMatrixTranslation(0.0f, 0.0f, -10.0f);

	float fov = DirectX::XMConvertToRadians(80);
	float aspect = (float)width / (float)height;
	m_viewToProjection = DirectX::XMMatrixPerspectiveFovLH(fov, aspect, 0.1f, 100.f);

	SimpleVertexCombined verticesCombo[] =
	{
		SimpleVertexCombined{ DirectX::XMFLOAT3(0.0f, 0.5f, 0.5f), DirectX::XMFLOAT3(0.0f, 0.0f, 0.5f) },
		SimpleVertexCombined{ DirectX::XMFLOAT3(0.5f, -0.5f, 0.5f), DirectX::XMFLOAT3(0.5f, 0.0f, 0.0f) },
		SimpleVertexCombined{ DirectX::XMFLOAT3(-0.5f, -0.5f, 0.5f), DirectX::XMFLOAT3(0.0f, 0.5f, 0.0f) },
	};

	D3D11_BUFFER_DESC vertBufferDesc = {
		.ByteWidth = sizeof(SimpleVertexCombined) * 3,
		.Usage = D3D11_USAGE_DEFAULT,
		.BindFlags = D3D11_BIND_VERTEX_BUFFER,
		.CPUAccessFlags = 0,
		.MiscFlags = 0,
		.StructureByteStride = sizeof(SimpleVertexCombined),
	};

	D3D11_SUBRESOURCE_DATA vertexBufferInitData = {
		.pSysMem = verticesCombo,
		// these have no meaning for vertex buffers
		.SysMemPitch = 0,
		.SysMemSlicePitch = 0,
	};
	
	if (auto res = m_device->CreateBuffer(&vertBufferDesc, &vertexBufferInitData, &m_vertexBuffer); FAILED(res)) {
		DXERROR(res);
	}

	int indices[] = {
		0, 1, 2
	};

	D3D11_BUFFER_DESC indexBufferDesc = {
		.ByteWidth = sizeof(int) * 3,
		.Usage = D3D11_USAGE_DEFAULT,
		.BindFlags = D3D11_BIND_INDEX_BUFFER,
		.CPUAccessFlags = 0,
		.MiscFlags = 0,
		.StructureByteStride = sizeof(int),
	};

	D3D11_SUBRESOURCE_DATA indexBufferInitData = {
		.pSysMem = indices,
		// these have no meaning for index buffers
		.SysMemPitch = 0,
		.SysMemSlicePitch = 0,
	};
	
	if (auto res = m_device->CreateBuffer(&indexBufferDesc, &indexBufferInitData, &m_indexBuffer); FAILED(res)) {
		DXERROR(res);
	}

	//D3D11_INPUT_ELEMENT_DESC CreateInputLayout = {
	//	LPCSTR SemanticName;
	//	UINT SemanticIndex;
	//	DXGI_FORMAT Format;
	//	UINT InputSlot;
	//	UINT AlignedByteOffset;
	//	D3D11_INPUT_CLASSIFICATION InputSlotClass;
	//	UINT InstanceDataStepRate;
	//};
	// m_device->CreateInputLayout(&inputElementDesc, 3, )
}

Renderer::~Renderer()
{
}

void Renderer::EnumAdapters(std::vector<ComPtr<IDXGIAdapter>>& outAdapters) {
	// hard upper bound 512, who has more than 512 gpus?
	for (int i = 0; i < 512; ++i)
	{
		ComPtr<IDXGIAdapter> adapter;
		HRESULT res = m_factory->EnumAdapters(i, &adapter);

		if (res == DXGI_ERROR_NOT_FOUND) {
			break;
		}

		outAdapters.push_back(adapter);
	}
}

Renderer::ComPtr<IDXGIAdapter> Renderer::PickAdapter(const std::vector<ComPtr<IDXGIAdapter>>& adapters) {
	spdlog::info("picking adapter");

	DXGI_ADAPTER_DESC desc;

	for (auto& adapter : adapters) {
		if (auto res = adapter->GetDesc(&desc); FAILED(res)) {
			DXERROR(res);
		}

		char description[128];
		memset(description, 0, 128);
		wcstombs(description, desc.Description, 128);

		// @TODO: format in multiline
		spdlog::info(
			"[DXGI_ADAPTER_DESC1 Description={} VendorId={} DeviceId={} SubSysId={} Revision={} DedicatedVideoMemory={} DedicatedSystemMemory={} SharedSystemMemory={}]",
			description,
			desc.VendorId,
			desc.DeviceId,
			desc.SubSysId,
			desc.Revision,
			desc.DedicatedVideoMemory,
			desc.DedicatedSystemMemory,
			desc.SharedSystemMemory
		);
	}

	// @TODO: pick a dedicated card, by scoring the gpus
	assert(adapters.size() > 0);
	// return first adapter for now
	return adapters[0];
}

void Renderer::EnumOutputs(ComPtr<IDXGIAdapter> adapter, std::vector<ComPtr<IDXGIOutput>>& outOutputs) {
	spdlog::info("enumerating outputs");
	outOutputs.reserve(8);

	ComPtr<IDXGIOutput> output;

	for (int i = 0; i < 512; ++i) {
		HRESULT res = adapter->EnumOutputs(i, &output);

		if (res == DXGI_ERROR_NOT_FOUND) {
			break;
		}

		outOutputs.push_back(output);
	}
}

Renderer::ComPtr<IDXGIOutput> Renderer::PickOutput(const std::vector<ComPtr<IDXGIOutput>>& outputs) {
	spdlog::info("picking outputs");

	for (auto& output : outputs) {
		DXGI_OUTPUT_DESC desc;
		if (auto res = output->GetDesc(&desc); FAILED(res)) {
			DXERROR(res);
		}

		char deviceName[128];
		memset(deviceName, 0, 128);
		wcstombs(deviceName, desc.DeviceName, 32);

		spdlog::info("[DXGI_OUTPUT_DESC DeviceName={} DesktopCoordinates.left={} DesktopCoordinates.top={} DesktopCoordinates.right={} DesktopCoordinates.bottom={} AttachedToDesktop={} Rotation={}]",
			deviceName,
			desc.DesktopCoordinates.left,
			desc.DesktopCoordinates.top,
			desc.DesktopCoordinates.right,
			desc.DesktopCoordinates.bottom,
			desc.AttachedToDesktop,
			(UINT)desc.Rotation
		);
	}

	// @TODO: pick output? based onwhat?
	return outputs[0];
}

void Renderer::CreateDeviceAndContext(UINT createFlags) {
	assert(m_selectedAdapter.Get() != nullptr);

	D3D_FEATURE_LEVEL requiredFeatureLevels[] = {
		D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_10_1,
		D3D_FEATURE_LEVEL_10_0,
		D3D_FEATURE_LEVEL_9_3,
		D3D_FEATURE_LEVEL_9_2,
		D3D_FEATURE_LEVEL_9_1,
	};

	D3D_FEATURE_LEVEL supportedFeatureLevel = (D3D_FEATURE_LEVEL)0;

	HRESULT res = D3D11CreateDevice(
		m_selectedAdapter.Get(),
		D3D_DRIVER_TYPE::D3D_DRIVER_TYPE_UNKNOWN,
		nullptr,
		createFlags,
		requiredFeatureLevels,
		ARR_LEN(requiredFeatureLevels),
		D3D11_SDK_VERSION,
		&m_device,
		&supportedFeatureLevel,
		&m_deviceContext
	);

	if (FAILED(res)) {
		DXERROR(res);
	}

	assert(supportedFeatureLevel == D3D_FEATURE_LEVEL_11_0);
	spdlog::info("created device and device context");
}

void Renderer::CreateSwapchain(GLFWwindow* window, u32 bufferCount) {
	spdlog::info("creating swapchain for window: {}", glfwGetWindowTitle(window));

	HWND hwnd = glfwGetWin32Window(window);
	int width = 0, height = 0;
	glfwGetWindowSize(window, &width, &height);

	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {
		.Width = static_cast<UINT>(width),
		.Height = static_cast<UINT>(height),
		.Format = DXGI_FORMAT::DXGI_FORMAT_R8G8B8A8_UNORM,
		.Stereo = false,
		.SampleDesc = {
			.Count = 1,
			.Quality = 0,
		},
		.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
		.BufferCount = bufferCount,
		.Scaling = DXGI_SCALING::DXGI_SCALING_STRETCH,
		.SwapEffect = DXGI_SWAP_EFFECT::DXGI_SWAP_EFFECT_DISCARD,
		.AlphaMode = DXGI_ALPHA_MODE::DXGI_ALPHA_MODE_UNSPECIFIED,
		.Flags = 0,
	};

	// @TODO: fullscreen swapchain
	// @TODO: selected output?
	if (auto res = m_factory->CreateSwapChainForHwnd(m_device.Get(), hwnd, &swapChainDesc, nullptr, nullptr, &m_swapchain); FAILED(res)) {
		DXERROR(res);
	}

	spdlog::info("created swapchain");
}

void Renderer::Render() {
	// begin render
	const float clearColor[4] = {0.5f, 0.1f, 0.1f, 1.0f};
	m_deviceContext->ClearRenderTargetView(m_renderTargetView.Get(), clearColor);
	m_deviceContext->ClearDepthStencilView(m_depthStencilView.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);

	// render stuff
	//m_deviceContext->IASetVertexBuffers(0, 1, &m_vertexBuffer, nullptr, nullptr);
	//m_deviceContext->IASetIndexBuffer(m_indexBuffer.Get(), DXGI_FORMAT_R32_SINT, 0);

	//m_deviceContext->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// end render
	// vsync enabled
	m_swapchain->Present(0, 0);
	//spdlog::info("rendering");
}