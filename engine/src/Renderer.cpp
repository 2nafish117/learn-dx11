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
	ObtainSwapchainResources();

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

	if(auto res = m_device->CreateTexture2D(&depthStencilTexDesc, nullptr, &m_depthStencilTexture); FAILED(res)) {
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
	
	if(auto res = m_device->CreateDepthStencilView(m_depthStencilTexture.Get(), &depthStencilViewDesc, &m_depthStencilView); FAILED(res)) {
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
		.Width = static_cast<float>(width),
		.Height = static_cast<float>(height),
		.MinDepth = 0.0f,
		.MaxDepth = 1.0f,
	};

	m_deviceContext->RSSetViewports(1, &m_viewport);

	// triangle
	//SimpleVertexCombined verticesCombo[] =
	//{
	//	SimpleVertexCombined{ DirectX::XMFLOAT3(0.0f, 0.5f, 0.5f), DirectX::XMFLOAT3(0.0f, 0.0f, 0.5f), DirectX::XMFLOAT2(0.0f, 0.5f) },
	//	SimpleVertexCombined{ DirectX::XMFLOAT3(0.5f, -0.5f, 0.5f), DirectX::XMFLOAT3(0.5f, 0.0f, 0.0f), DirectX::XMFLOAT2(0.5f, -0.5f) },
	//	SimpleVertexCombined{ DirectX::XMFLOAT3(-0.5f, -0.5f, 0.5f), DirectX::XMFLOAT3(0.0f, 0.5f, 0.0f), DirectX::XMFLOAT2(-0.5f, -0.5f) },
	//};

	// quad
	SimpleVertexCombined verticesCombo[] =
	{
		SimpleVertexCombined{ DirectX::XMFLOAT3(-0.5f, -0.5f, 0.5f), DirectX::XMFLOAT3(0.0f, 0.0f, -1.0f), DirectX::XMFLOAT3(0.0f, 0.5f, 0.0f), DirectX::XMFLOAT2(0.0f, 1.0f) },
		SimpleVertexCombined{ DirectX::XMFLOAT3(0.5f, 0.5f, 0.5f), DirectX::XMFLOAT3(0.0f, 0.0f, -1.0f), DirectX::XMFLOAT3(0.0f, 0.0f, 0.5f), DirectX::XMFLOAT2(1.0f, 0.0f) },
		SimpleVertexCombined{ DirectX::XMFLOAT3(0.5f, -0.5f, 0.5f), DirectX::XMFLOAT3(0.0f, 0.0f, -1.0f), DirectX::XMFLOAT3(0.0f, 0.0f, 0.5f), DirectX::XMFLOAT2(1.0f, 1.0f) },
		SimpleVertexCombined{ DirectX::XMFLOAT3(-0.5f, 0.5f, 0.5f), DirectX::XMFLOAT3(0.0f, 0.0f, -1.0f), DirectX::XMFLOAT3(0.5f, 0.0f, 0.0f), DirectX::XMFLOAT2(0.0f, 0.0f) },
	};


	D3D11_BUFFER_DESC vertBufferDesc = {
		.ByteWidth = sizeof(SimpleVertexCombined) * ARRAYSIZE(verticesCombo),
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

	// triangle
	//u32 indices[] = {
	//	0, 1, 2
	//};

	// quad
	u32 indices[] = {
		0, 1, 2,
		0, 3, 1
	};

	D3D11_BUFFER_DESC indexBufferDesc = {
		.ByteWidth = sizeof(int) * ARRAYSIZE(indices),
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

	// @TODO: shader defines and includes
	// D3D_SHADER_MACRO defines = {
	// 	LPCSTR Name;
	// 	LPCSTR Definition;
	// };
	// I need to implement this interface to actually use it
	// ComPtr<ID3DInclude> include;

	ComPtr<ID3DBlob> vertexBytecode;
	{
		ComPtr<ID3DBlob> errors;

		UINT flags1 = D3DCOMPILE_WARNINGS_ARE_ERRORS | D3DCOMPILE_ENABLE_STRICTNESS;
		// something to do with fx files???
		UINT flags2 = 0;

		if(auto res = D3DCompileFromFile(L"data/shaders/simple_vs.hlsl", nullptr, nullptr, "VSMain", "vs_5_0", flags1, flags2, &vertexBytecode, &errors); FAILED(res)) {
			DXERROR(res);
			spdlog::error("shader compile error: {}", (const char*)errors->GetBufferPointer());
		}
	}

	ComPtr<ID3DBlob> pixelBytecode;
	{
		ComPtr<ID3DBlob> errors;

		UINT flags1 = D3DCOMPILE_WARNINGS_ARE_ERRORS | D3DCOMPILE_ENABLE_STRICTNESS;
		// something to do with fx files???
		UINT flags2 = 0;

		if (auto res = D3DCompileFromFile(L"data/shaders/simple_ps.hlsl", nullptr, nullptr, "PSMain", "ps_5_0", flags1, flags2, &pixelBytecode, &errors); FAILED(res)) {
			DXERROR(res);
			spdlog::error("shader compile error: {}", (const char*)errors->GetBufferPointer());
		}
	}

	m_device->CreateVertexShader(vertexBytecode->GetBufferPointer(), vertexBytecode->GetBufferSize(), nullptr, &m_simpleVertex);

	m_device->CreatePixelShader(pixelBytecode->GetBufferPointer(), pixelBytecode->GetBufferSize(), nullptr, &m_simplePixel);

	D3D11_INPUT_ELEMENT_DESC inputElementDescs[] = {
		{
			.SemanticName = "POSITION",
			.SemanticIndex = 0,
			.Format = DXGI_FORMAT::DXGI_FORMAT_R32G32B32_FLOAT,
			.InputSlot = 0,
			.AlignedByteOffset = 0,
			.InputSlotClass = D3D11_INPUT_CLASSIFICATION::D3D11_INPUT_PER_VERTEX_DATA,
			.InstanceDataStepRate = 0,
		},
		{
			.SemanticName = "NORMAL",
			.SemanticIndex = 0,
			.Format = DXGI_FORMAT::DXGI_FORMAT_R32G32B32_FLOAT,
			.InputSlot = 0,
			.AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT,
			.InputSlotClass = D3D11_INPUT_CLASSIFICATION::D3D11_INPUT_PER_VERTEX_DATA,
			.InstanceDataStepRate = 0,
		},
		{
			.SemanticName = "COLOR",
			.SemanticIndex = 0,
			.Format = DXGI_FORMAT::DXGI_FORMAT_R32G32B32_FLOAT,
			.InputSlot = 0,
			.AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT,
			.InputSlotClass = D3D11_INPUT_CLASSIFICATION::D3D11_INPUT_PER_VERTEX_DATA,
			.InstanceDataStepRate = 0,
		},
		{
			.SemanticName = "TEXCOORD",
			.SemanticIndex = 0,
			.Format = DXGI_FORMAT::DXGI_FORMAT_R32G32_FLOAT,
			.InputSlot = 0,
			.AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT,
			.InputSlotClass = D3D11_INPUT_CLASSIFICATION::D3D11_INPUT_PER_VERTEX_DATA,
			.InstanceDataStepRate = 0,
		}
	};

	ComPtr<ID3D11InputLayout> m_inputLayout;
	m_device->CreateInputLayout(inputElementDescs, ARRAYSIZE(inputElementDescs), vertexBytecode->GetBufferPointer(), vertexBytecode->GetBufferSize(), &m_inputLayout);
	m_deviceContext->IASetInputLayout(m_inputLayout.Get());

	int texture_width = 0;
	int texture_height = 0;
	int channels = 0;
	stbi_uc* data = stbi_load("data/textures/placeholder.png", &texture_width, &texture_height, &channels, 4);

	ComPtr<ID3D11Texture2D> m_testTexture;

	D3D11_TEXTURE2D_DESC testTextureDesc = {
		.Width = static_cast<UINT>(texture_width),
		.Height = static_cast<UINT>(texture_height),
		// @TODO: 1 for multisampled???
		.MipLevels = 1,
		.ArraySize = 1,
		.Format = DXGI_FORMAT::DXGI_FORMAT_R8G8B8A8_UNORM,
		.SampleDesc = {
			.Count = 1,
			.Quality = 0,
		},
		.Usage = D3D11_USAGE::D3D11_USAGE_IMMUTABLE,
		.BindFlags = D3D11_BIND_FLAG::D3D11_BIND_SHADER_RESOURCE,
		.CPUAccessFlags = 0,
	};

	D3D11_SUBRESOURCE_DATA testSubresourceData = {
		.pSysMem = data,
		.SysMemPitch = (u32)texture_width * channels * sizeof(byte),
		.SysMemSlicePitch = 0,
	};

	if(auto res = m_device->CreateTexture2D(&testTextureDesc, &testSubresourceData, &m_testTexture); FAILED(res)) {
		DXERROR(res);
	}

	D3D11_SHADER_RESOURCE_VIEW_DESC testTextureSRVDesc = {
		.Format = DXGI_FORMAT::DXGI_FORMAT_R8G8B8A8_UNORM,
    	.ViewDimension = D3D11_SRV_DIMENSION::D3D11_SRV_DIMENSION_TEXTURE2D,
		.Texture2D = {
			.MostDetailedMip = 0,
    		.MipLevels = static_cast<UINT>(-1),
		}
	};

	if(auto res = m_device->CreateShaderResourceView(m_testTexture.Get(), &testTextureSRVDesc, &m_testSRV); FAILED(res)) {
		DXERROR(res);
	}

	D3D11_SAMPLER_DESC testSamplerStateDesc = {
		.Filter = D3D11_FILTER::D3D11_FILTER_MIN_MAG_MIP_LINEAR,
		.AddressU = D3D11_TEXTURE_ADDRESS_MODE::D3D11_TEXTURE_ADDRESS_WRAP,
		.AddressV = D3D11_TEXTURE_ADDRESS_MODE::D3D11_TEXTURE_ADDRESS_WRAP,
		.AddressW = D3D11_TEXTURE_ADDRESS_MODE::D3D11_TEXTURE_ADDRESS_WRAP,
		.MipLODBias = 0.0f,
		.MaxAnisotropy = 1,
		.ComparisonFunc = D3D11_COMPARISON_FUNC::D3D11_COMPARISON_NEVER,
		.BorderColor = {0, 0, 0, 0},
		.MinLOD = 0.0f,
		.MaxLOD = D3D11_FLOAT32_MAX,
	};

	if(auto res = m_device->CreateSamplerState(&testSamplerStateDesc, &m_testSamplerState); FAILED(res)) {
		DXERROR(res);
	}

	D3D11_BUFFER_DESC matrixBufferDesc = {
		.ByteWidth = sizeof(MatrixBuffer),
		.Usage = D3D11_USAGE::D3D11_USAGE_DYNAMIC,
		.BindFlags = D3D11_BIND_CONSTANT_BUFFER,
		.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE,
		.MiscFlags = 0,
		.StructureByteStride = 0,
	};
	
	if (auto res = m_device->CreateBuffer(&matrixBufferDesc, nullptr, &m_matrixBuffer); FAILED(res)) {
		DXERROR(res);
	}

	D3D11_BUFFER_DESC pointLightBufferDesc = {
		.ByteWidth = sizeof(PointLightBuffer),
		.Usage = D3D11_USAGE::D3D11_USAGE_DYNAMIC,
		.BindFlags = D3D11_BIND_CONSTANT_BUFFER,
		.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE,
		.MiscFlags = 0,
		.StructureByteStride = 0,
	};
	
	if (auto res = m_device->CreateBuffer(&pointLightBufferDesc, nullptr, &m_pointLightBuffer); FAILED(res)) {
		DXERROR(res);
	}
}

Renderer::~Renderer()
{
#ifdef _DEBUG
	if (auto res = m_debug->ReportLiveDeviceObjects(D3D11_RLDO_FLAGS::D3D11_RLDO_DETAIL); FAILED(res)) {
		DXERROR(res);
	}
#endif
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

void Renderer::ObtainSwapchainResources() {
	ComPtr<ID3D11Texture2D> backBuffer;
	if (auto res = m_swapchain->GetBuffer(0, IID_PPV_ARGS(&backBuffer)); FAILED(res)) {
		DXERROR(res);
	}

	spdlog::info("obtained buffer from swapchain");

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
}

void Renderer::ReleaseSwapchainResources() {
	m_renderTargetView.Reset();
	spdlog::info("release swapchain resources");
}

void Renderer::ResizeSwapchainResources(u32 width, u32 height) {
	m_deviceContext->Flush();
	ReleaseSwapchainResources();

	if(auto res = m_swapchain->ResizeBuffers(1, width, height, DXGI_FORMAT::DXGI_FORMAT_UNKNOWN, 0); FAILED(res)) {
		DXERROR(res);
	}

	spdlog::info("resized swapchain buffers");

	ObtainSwapchainResources();
}

void Renderer::Render() {

	// begin render
	const float clearColor[4] = {0.1f, 0.1f, 0.1f, 1.0f};
	m_deviceContext->ClearRenderTargetView(m_renderTargetView.Get(), clearColor);
	m_deviceContext->ClearDepthStencilView(m_depthStencilView.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);


	double time = glfwGetTime();
	double angle = DirectX::XMScalarSin(time);
	double moveX = 0.5 * DirectX::XMScalarSin(time * 2);
	double moveY = 0.5 * DirectX::XMScalarSin(time * 2);
	auto rotMatrix = DirectX::XMMatrixRotationY(angle);
	//auto rotMatrix = DirectX::XMMatrixTranslation(angle, 0, 0);
	//auto rotMatrix = DirectX::XMMatrixIdentity();

	// @TODO: camera class
	float fov = DirectX::XMConvertToRadians(80);
	float aspect = (float)16.0 / (float)9.0;

	auto transMatrix = DirectX::XMMatrixTranslation(moveX, moveY, 0);

	DirectX::XMMATRIX modelToWorld = DirectX::XMMatrixIdentity();
	modelToWorld = modelToWorld * rotMatrix;
	modelToWorld = modelToWorld * transMatrix;

	// worl to view is the inverse of the model to world matrix of the camera
	DirectX::XMMATRIX worldToView = DirectX::XMMatrixInverse(nullptr, DirectX::XMMatrixTranslation(0, 0, -1));
	DirectX::XMMATRIX viewToProjection = DirectX::XMMatrixPerspectiveFovLH(fov, aspect, 0.1f, 100.0f);

	D3D11_MAPPED_SUBRESOURCE subresource;
	m_deviceContext->Map(m_matrixBuffer.Get(), 0, D3D11_MAP::D3D11_MAP_WRITE_DISCARD, 0, &subresource);
	MatrixBuffer* data = reinterpret_cast<MatrixBuffer*>(subresource.pData);
	data->ModelToWorld = DirectX::XMMatrixTranspose(modelToWorld);
	data->WorldToView = DirectX::XMMatrixTranspose(worldToView);
	data->ViewToProjection = DirectX::XMMatrixTranspose(viewToProjection);
	m_deviceContext->Unmap(m_matrixBuffer.Get(), 0);

	D3D11_MAPPED_SUBRESOURCE pointSubresource;
	m_deviceContext->Map(m_pointLightBuffer.Get(), 0, D3D11_MAP::D3D11_MAP_WRITE_DISCARD, 0, &pointSubresource);
	PointLightBuffer* plBuffer = reinterpret_cast<PointLightBuffer*>(pointSubresource.pData);
	plBuffer->Pos = DirectX::XMFLOAT3(0.0, 0.0f, 0);
	plBuffer->Col = DirectX::XMFLOAT3(1.0, 1.0, 1.0);
	m_deviceContext->Unmap(m_pointLightBuffer.Get(), 0);

	// render stuff
	UINT strides[] = { sizeof(SimpleVertexCombined) };
	UINT offsets[] = { 0 };
	m_deviceContext->IASetVertexBuffers(0, 1, m_vertexBuffer.GetAddressOf(), strides, offsets);
	m_deviceContext->IASetIndexBuffer(m_indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
	m_deviceContext->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	
	m_deviceContext->VSSetShader(m_simpleVertex.Get(), nullptr, 0);
	m_deviceContext->VSSetConstantBuffers(0, 1, m_matrixBuffer.GetAddressOf());

	m_deviceContext->PSSetShader(m_simplePixel.Get(), nullptr, 0);
	m_deviceContext->PSSetShaderResources(0, 1, m_testSRV.GetAddressOf());
	m_deviceContext->PSSetSamplers(0, 1, m_testSamplerState.GetAddressOf());
	m_deviceContext->PSSetConstantBuffers(0, 1, m_pointLightBuffer.GetAddressOf());
	m_deviceContext->PSSetConstantBuffers(1, 1, m_matrixBuffer.GetAddressOf());

	m_deviceContext->OMSetRenderTargets(1, m_renderTargetView.GetAddressOf(), m_depthStencilView.Get());

	m_deviceContext->DrawIndexed(6, 0, 0);

	// end render
	// vsync enabled
	m_swapchain->Present(0, 0);
	//spdlog::info("rendering");
}

void Renderer::HandleResize(u32 width, u32 height)
{
	ResizeSwapchainResources(width, height);


	//m_depthStencilView.Reset();
	//m_depthStencilTexture.Reset();
	//m_device->CreateTexture2D()
}
