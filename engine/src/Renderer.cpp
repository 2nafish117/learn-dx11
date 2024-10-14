#include "Renderer.hpp"

#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include <imgui.h>
#include <backends/imgui_impl_dx11.h>
#include <backends/imgui_impl_glfw.h>

#include <comdef.h>

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

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // IF using Docking Branch

	ImGui_ImplGlfw_InitForOther(m_window, true);
	ImGui_ImplDX11_Init(GetDevice().Get(), GetDeviceContext().Get());
}

Renderer::~Renderer()
{
#ifdef _DEBUG
	// @TODO: this is kinda stupid because the lifetimes of the dx objects are tied to the lifetimes of the renderer
	// and running this at in the destructor of renderer means they havent been destroyed yet
	// so it will report that all objects are still alive, where should i run this then? pull it out of the renderer?
	if (auto res = m_debug->ReportLiveDeviceObjects(D3D11_RLDO_FLAGS::D3D11_RLDO_DETAIL); FAILED(res)) {
		DXERROR(res);
	}
#endif

	ImGui_ImplDX11_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
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

#if _DEBUG
	m_device->QueryInterface(IID_PPV_ARGS(&m_debugInfoQueue));
#endif
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


	ImGui_ImplDX11_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();
	ImGui::ShowDemoWindow();


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

	// Rendering
	// (Your code clears your framebuffer, renders your other stuff etc.)
	ImGui::Render();

	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
	// (Your code calls swapchain's Present() function)

	// end render
	// vsync enabled
	m_swapchain->Present(0, 0);
	//spdlog::info("rendering");

	LogDebugInfo();
}

void Renderer::HandleResize(u32 width, u32 height)
{
	ResizeSwapchainResources(width, height);


	//m_depthStencilView.Reset();
	//m_depthStencilTexture.Reset();
	//m_device->CreateTexture2D()
}

void Renderer::LogDebugInfo()
{
	static const char* messageCategoryStrings[] = {
		"APPLICATION_DEFINED",
        "MISCELLANEOUS",
        "INITIALIZATION",
        "CLEANUP",
        "COMPILATION",
        "STATE_CREATION",
        "STATE_SETTING",
        "STATE_GETTING",
        "RESOURCE_MANIPULATION",
        "EXECUTION",
        "SHADER"
	};

	static const char* messageSeverityStrings[] = {
		"CORRUPTION",
        "ERROR",
        "WARNING",
        "INFO",
        "MESSAGE" 
	};

	static const char* messageIdStrings[] = {
		"UNKNOWN",
		"DEVICE_IASETVERTEXBUFFERS_HAZARD",
		"DEVICE_IASETINDEXBUFFER_HAZARD",
		"DEVICE_VSSETSHADERRESOURCES_HAZARD",
		"DEVICE_VSSETCONSTANTBUFFERS_HAZARD",
		"DEVICE_GSSETSHADERRESOURCES_HAZARD",
		"DEVICE_GSSETCONSTANTBUFFERS_HAZARD",
		"DEVICE_PSSETSHADERRESOURCES_HAZARD",
		"DEVICE_PSSETCONSTANTBUFFERS_HAZARD",
		"DEVICE_OMSETRENDERTARGETS_HAZARD",
		"DEVICE_SOSETTARGETS_HAZARD",
		"STRING_FROM_APPLICATION",
		"CORRUPTED_THIS",
		"CORRUPTED_PARAMETER1",
		"CORRUPTED_PARAMETER2",
		"CORRUPTED_PARAMETER3",
		"CORRUPTED_PARAMETER4",
		"CORRUPTED_PARAMETER5",
		"CORRUPTED_PARAMETER6",
		"CORRUPTED_PARAMETER7",
		"CORRUPTED_PARAMETER8",
		"CORRUPTED_PARAMETER9",
		"CORRUPTED_PARAMETER10",
		"CORRUPTED_PARAMETER11",
		"CORRUPTED_PARAMETER12",
		"CORRUPTED_PARAMETER13",
		"CORRUPTED_PARAMETER14",
		"CORRUPTED_PARAMETER15",
		"CORRUPTED_MULTITHREADING",
		"MESSAGE_REPORTING_OUTOFMEMORY",
		"IASETINPUTLAYOUT_UNBINDDELETINGOBJECT",
		"IASETVERTEXBUFFERS_UNBINDDELETINGOBJECT",
		"IASETINDEXBUFFER_UNBINDDELETINGOBJECT",
		"VSSETSHADER_UNBINDDELETINGOBJECT",
		"VSSETSHADERRESOURCES_UNBINDDELETINGOBJECT",
		"VSSETCONSTANTBUFFERS_UNBINDDELETINGOBJECT",
		"VSSETSAMPLERS_UNBINDDELETINGOBJECT",
		"GSSETSHADER_UNBINDDELETINGOBJECT",
		"GSSETSHADERRESOURCES_UNBINDDELETINGOBJECT",
		"GSSETCONSTANTBUFFERS_UNBINDDELETINGOBJECT",
		"GSSETSAMPLERS_UNBINDDELETINGOBJECT",
		"SOSETTARGETS_UNBINDDELETINGOBJECT",
		"PSSETSHADER_UNBINDDELETINGOBJECT",
		"PSSETSHADERRESOURCES_UNBINDDELETINGOBJECT",
		"PSSETCONSTANTBUFFERS_UNBINDDELETINGOBJECT",
		"PSSETSAMPLERS_UNBINDDELETINGOBJECT",
		"RSSETSTATE_UNBINDDELETINGOBJECT",
		"OMSETBLENDSTATE_UNBINDDELETINGOBJECT",
		"OMSETDEPTHSTENCILSTATE_UNBINDDELETINGOBJECT",
		"OMSETRENDERTARGETS_UNBINDDELETINGOBJECT",
		"SETPREDICATION_UNBINDDELETINGOBJECT",
		"GETPRIVATEDATA_MOREDATA",
		"SETPRIVATEDATA_INVALIDFREEDATA",
		"SETPRIVATEDATA_INVALIDIUNKNOWN",
		"SETPRIVATEDATA_INVALIDFLAGS",
		"SETPRIVATEDATA_CHANGINGPARAMS",
		"SETPRIVATEDATA_OUTOFMEMORY",
		"CREATEBUFFER_UNRECOGNIZEDFORMAT",
		"CREATEBUFFER_INVALIDSAMPLES",
		"CREATEBUFFER_UNRECOGNIZEDUSAGE",
		"CREATEBUFFER_UNRECOGNIZEDBINDFLAGS",
		"CREATEBUFFER_UNRECOGNIZEDCPUACCESSFLAGS",
		"CREATEBUFFER_UNRECOGNIZEDMISCFLAGS",
		"CREATEBUFFER_INVALIDCPUACCESSFLAGS",
		"CREATEBUFFER_INVALIDBINDFLAGS",
		"CREATEBUFFER_INVALIDINITIALDATA",
		"CREATEBUFFER_INVALIDDIMENSIONS",
		"CREATEBUFFER_INVALIDMIPLEVELS",
		"CREATEBUFFER_INVALIDMISCFLAGS",
		"CREATEBUFFER_INVALIDARG_RETURN",
		"CREATEBUFFER_OUTOFMEMORY_RETURN",
		"CREATEBUFFER_NULLDESC",
		"CREATEBUFFER_INVALIDCONSTANTBUFFERBINDINGS",
		"CREATEBUFFER_LARGEALLOCATION",
		"CREATETEXTURE1D_UNRECOGNIZEDFORMAT",
		"CREATETEXTURE1D_UNSUPPORTEDFORMAT",
		"CREATETEXTURE1D_INVALIDSAMPLES",
		"CREATETEXTURE1D_UNRECOGNIZEDUSAGE",
		"CREATETEXTURE1D_UNRECOGNIZEDBINDFLAGS",
		"CREATETEXTURE1D_UNRECOGNIZEDCPUACCESSFLAGS",
		"CREATETEXTURE1D_UNRECOGNIZEDMISCFLAGS",
		"CREATETEXTURE1D_INVALIDCPUACCESSFLAGS",
		"CREATETEXTURE1D_INVALIDBINDFLAGS",
		"CREATETEXTURE1D_INVALIDINITIALDATA",
		"CREATETEXTURE1D_INVALIDDIMENSIONS",
		"CREATETEXTURE1D_INVALIDMIPLEVELS",
		"CREATETEXTURE1D_INVALIDMISCFLAGS",
		"CREATETEXTURE1D_INVALIDARG_RETURN",
		"CREATETEXTURE1D_OUTOFMEMORY_RETURN",
		"CREATETEXTURE1D_NULLDESC",
		"CREATETEXTURE1D_LARGEALLOCATION",
		"CREATETEXTURE2D_UNRECOGNIZEDFORMAT",
		"CREATETEXTURE2D_UNSUPPORTEDFORMAT",
		"CREATETEXTURE2D_INVALIDSAMPLES",
		"CREATETEXTURE2D_UNRECOGNIZEDUSAGE",
		"CREATETEXTURE2D_UNRECOGNIZEDBINDFLAGS",
		"CREATETEXTURE2D_UNRECOGNIZEDCPUACCESSFLAGS",
		"CREATETEXTURE2D_UNRECOGNIZEDMISCFLAGS",
		"CREATETEXTURE2D_INVALIDCPUACCESSFLAGS",
		"CREATETEXTURE2D_INVALIDBINDFLAGS",
		"CREATETEXTURE2D_INVALIDINITIALDATA",
		"CREATETEXTURE2D_INVALIDDIMENSIONS",
		"CREATETEXTURE2D_INVALIDMIPLEVELS",
		"CREATETEXTURE2D_INVALIDMISCFLAGS",
		"CREATETEXTURE2D_INVALIDARG_RETURN",
		"CREATETEXTURE2D_OUTOFMEMORY_RETURN",
		"CREATETEXTURE2D_NULLDESC",
		"CREATETEXTURE2D_LARGEALLOCATION",
		"CREATETEXTURE3D_UNRECOGNIZEDFORMAT",
		"CREATETEXTURE3D_UNSUPPORTEDFORMAT",
		"CREATETEXTURE3D_INVALIDSAMPLES",
		"CREATETEXTURE3D_UNRECOGNIZEDUSAGE",
		"CREATETEXTURE3D_UNRECOGNIZEDBINDFLAGS",
		"CREATETEXTURE3D_UNRECOGNIZEDCPUACCESSFLAGS",
		"CREATETEXTURE3D_UNRECOGNIZEDMISCFLAGS",
		"CREATETEXTURE3D_INVALIDCPUACCESSFLAGS",
		"CREATETEXTURE3D_INVALIDBINDFLAGS",
		"CREATETEXTURE3D_INVALIDINITIALDATA",
		"CREATETEXTURE3D_INVALIDDIMENSIONS",
		"CREATETEXTURE3D_INVALIDMIPLEVELS",
		"CREATETEXTURE3D_INVALIDMISCFLAGS",
		"CREATETEXTURE3D_INVALIDARG_RETURN",
		"CREATETEXTURE3D_OUTOFMEMORY_RETURN",
		"CREATETEXTURE3D_NULLDESC",
		"CREATETEXTURE3D_LARGEALLOCATION",
		"CREATESHADERRESOURCEVIEW_UNRECOGNIZEDFORMAT",
		"CREATESHADERRESOURCEVIEW_INVALIDDESC",
		"CREATESHADERRESOURCEVIEW_INVALIDFORMAT",
		"CREATESHADERRESOURCEVIEW_INVALIDDIMENSIONS",
		"CREATESHADERRESOURCEVIEW_INVALIDRESOURCE",
		"CREATESHADERRESOURCEVIEW_TOOMANYOBJECTS",
		"CREATESHADERRESOURCEVIEW_INVALIDARG_RETURN",
		"CREATESHADERRESOURCEVIEW_OUTOFMEMORY_RETURN",
		"CREATERENDERTARGETVIEW_UNRECOGNIZEDFORMAT",
		"CREATERENDERTARGETVIEW_UNSUPPORTEDFORMAT",
		"CREATERENDERTARGETVIEW_INVALIDDESC",
		"CREATERENDERTARGETVIEW_INVALIDFORMAT",
		"CREATERENDERTARGETVIEW_INVALIDDIMENSIONS",
		"CREATERENDERTARGETVIEW_INVALIDRESOURCE",
		"CREATERENDERTARGETVIEW_TOOMANYOBJECTS",
		"CREATERENDERTARGETVIEW_INVALIDARG_RETURN",
		"CREATERENDERTARGETVIEW_OUTOFMEMORY_RETURN",
		"CREATEDEPTHSTENCILVIEW_UNRECOGNIZEDFORMAT",
		"CREATEDEPTHSTENCILVIEW_INVALIDDESC",
		"CREATEDEPTHSTENCILVIEW_INVALIDFORMAT",
		"CREATEDEPTHSTENCILVIEW_INVALIDDIMENSIONS",
		"CREATEDEPTHSTENCILVIEW_INVALIDRESOURCE",
		"CREATEDEPTHSTENCILVIEW_TOOMANYOBJECTS",
		"CREATEDEPTHSTENCILVIEW_INVALIDARG_RETURN",
		"CREATEDEPTHSTENCILVIEW_OUTOFMEMORY_RETURN",
		"CREATEINPUTLAYOUT_OUTOFMEMORY",
		"CREATEINPUTLAYOUT_TOOMANYELEMENTS",
		"CREATEINPUTLAYOUT_INVALIDFORMAT",
		"CREATEINPUTLAYOUT_INCOMPATIBLEFORMAT",
		"CREATEINPUTLAYOUT_INVALIDSLOT",
		"CREATEINPUTLAYOUT_INVALIDINPUTSLOTCLASS",
		"CREATEINPUTLAYOUT_STEPRATESLOTCLASSMISMATCH",
		"CREATEINPUTLAYOUT_INVALIDSLOTCLASSCHANGE",
		"CREATEINPUTLAYOUT_INVALIDSTEPRATECHANGE",
		"CREATEINPUTLAYOUT_INVALIDALIGNMENT",
		"CREATEINPUTLAYOUT_DUPLICATESEMANTIC",
		"CREATEINPUTLAYOUT_UNPARSEABLEINPUTSIGNATURE",
		"CREATEINPUTLAYOUT_NULLSEMANTIC",
		"CREATEINPUTLAYOUT_MISSINGELEMENT",
		"CREATEINPUTLAYOUT_NULLDESC",
		"CREATEVERTEXSHADER_OUTOFMEMORY",
		"CREATEVERTEXSHADER_INVALIDSHADERBYTECODE",
		"CREATEVERTEXSHADER_INVALIDSHADERTYPE",
		"CREATEGEOMETRYSHADER_OUTOFMEMORY",
		"CREATEGEOMETRYSHADER_INVALIDSHADERBYTECODE",
		"CREATEGEOMETRYSHADER_INVALIDSHADERTYPE",
		"CREATEGEOMETRYSHADERWITHSTREAMOUTPUT_OUTOFMEMORY",
		"CREATEGEOMETRYSHADERWITHSTREAMOUTPUT_INVALIDSHADERBYTECODE",
		"CREATEGEOMETRYSHADERWITHSTREAMOUTPUT_INVALIDSHADERTYPE",
		"CREATEGEOMETRYSHADERWITHSTREAMOUTPUT_INVALIDNUMENTRIES",
		"CREATEGEOMETRYSHADERWITHSTREAMOUTPUT_OUTPUTSTREAMSTRIDEUNUSED",
		"CREATEGEOMETRYSHADERWITHSTREAMOUTPUT_UNEXPECTEDDECL",
		"CREATEGEOMETRYSHADERWITHSTREAMOUTPUT_EXPECTEDDECL",
		"CREATEGEOMETRYSHADERWITHSTREAMOUTPUT_OUTPUTSLOT0EXPECTED",
		"CREATEGEOMETRYSHADERWITHSTREAMOUTPUT_INVALIDOUTPUTSLOT",
		"CREATEGEOMETRYSHADERWITHSTREAMOUTPUT_ONLYONEELEMENTPERSLOT",
		"CREATEGEOMETRYSHADERWITHSTREAMOUTPUT_INVALIDCOMPONENTCOUNT",
		"CREATEGEOMETRYSHADERWITHSTREAMOUTPUT_INVALIDSTARTCOMPONENTANDCOMPONENTCOUNT",
		"CREATEGEOMETRYSHADERWITHSTREAMOUTPUT_INVALIDGAPDEFINITION",
		"CREATEGEOMETRYSHADERWITHSTREAMOUTPUT_REPEATEDOUTPUT",
		"CREATEGEOMETRYSHADERWITHSTREAMOUTPUT_INVALIDOUTPUTSTREAMSTRIDE",
		"CREATEGEOMETRYSHADERWITHSTREAMOUTPUT_MISSINGSEMANTIC",
		"CREATEGEOMETRYSHADERWITHSTREAMOUTPUT_MASKMISMATCH",
		"CREATEGEOMETRYSHADERWITHSTREAMOUTPUT_CANTHAVEONLYGAPS",
		"CREATEGEOMETRYSHADERWITHSTREAMOUTPUT_DECLTOOCOMPLEX",
		"CREATEGEOMETRYSHADERWITHSTREAMOUTPUT_MISSINGOUTPUTSIGNATURE",
		"CREATEPIXELSHADER_OUTOFMEMORY",
		"CREATEPIXELSHADER_INVALIDSHADERBYTECODE",
		"CREATEPIXELSHADER_INVALIDSHADERTYPE",
		"CREATERASTERIZERSTATE_INVALIDFILLMODE",
		"CREATERASTERIZERSTATE_INVALIDCULLMODE",
		"CREATERASTERIZERSTATE_INVALIDDEPTHBIASCLAMP",
		"CREATERASTERIZERSTATE_INVALIDSLOPESCALEDDEPTHBIAS",
		"CREATERASTERIZERSTATE_TOOMANYOBJECTS",
		"CREATERASTERIZERSTATE_NULLDESC",
		"CREATEDEPTHSTENCILSTATE_INVALIDDEPTHWRITEMASK",
		"CREATEDEPTHSTENCILSTATE_INVALIDDEPTHFUNC",
		"CREATEDEPTHSTENCILSTATE_INVALIDFRONTFACESTENCILFAILOP",
		"CREATEDEPTHSTENCILSTATE_INVALIDFRONTFACESTENCILZFAILOP",
		"CREATEDEPTHSTENCILSTATE_INVALIDFRONTFACESTENCILPASSOP",
		"CREATEDEPTHSTENCILSTATE_INVALIDFRONTFACESTENCILFUNC",
		"CREATEDEPTHSTENCILSTATE_INVALIDBACKFACESTENCILFAILOP",
		"CREATEDEPTHSTENCILSTATE_INVALIDBACKFACESTENCILZFAILOP",
		"CREATEDEPTHSTENCILSTATE_INVALIDBACKFACESTENCILPASSOP",
		"CREATEDEPTHSTENCILSTATE_INVALIDBACKFACESTENCILFUNC",
		"CREATEDEPTHSTENCILSTATE_TOOMANYOBJECTS",
		"CREATEDEPTHSTENCILSTATE_NULLDESC",
		"CREATEBLENDSTATE_INVALIDSRCBLEND",
		"CREATEBLENDSTATE_INVALIDDESTBLEND",
		"CREATEBLENDSTATE_INVALIDBLENDOP",
		"CREATEBLENDSTATE_INVALIDSRCBLENDALPHA",
		"CREATEBLENDSTATE_INVALIDDESTBLENDALPHA",
		"CREATEBLENDSTATE_INVALIDBLENDOPALPHA",
		"CREATEBLENDSTATE_INVALIDRENDERTARGETWRITEMASK",
		"CREATEBLENDSTATE_TOOMANYOBJECTS",
		"CREATEBLENDSTATE_NULLDESC",
		"CREATESAMPLERSTATE_INVALIDFILTER",
		"CREATESAMPLERSTATE_INVALIDADDRESSU",
		"CREATESAMPLERSTATE_INVALIDADDRESSV",
		"CREATESAMPLERSTATE_INVALIDADDRESSW",
		"CREATESAMPLERSTATE_INVALIDMIPLODBIAS",
		"CREATESAMPLERSTATE_INVALIDMAXANISOTROPY",
		"CREATESAMPLERSTATE_INVALIDCOMPARISONFUNC",
		"CREATESAMPLERSTATE_INVALIDMINLOD",
		"CREATESAMPLERSTATE_INVALIDMAXLOD",
		"CREATESAMPLERSTATE_TOOMANYOBJECTS",
		"CREATESAMPLERSTATE_NULLDESC",
		"CREATEQUERYORPREDICATE_INVALIDQUERY",
		"CREATEQUERYORPREDICATE_INVALIDMISCFLAGS",
		"CREATEQUERYORPREDICATE_UNEXPECTEDMISCFLAG",
		"CREATEQUERYORPREDICATE_NULLDESC",
		"DEVICE_IASETPRIMITIVETOPOLOGY_TOPOLOGY_UNRECOGNIZED",
		"DEVICE_IASETPRIMITIVETOPOLOGY_TOPOLOGY_UNDEFINED",
		"IASETVERTEXBUFFERS_INVALIDBUFFER",
		"DEVICE_IASETVERTEXBUFFERS_OFFSET_TOO_LARGE",
		"DEVICE_IASETVERTEXBUFFERS_BUFFERS_EMPTY",
		"IASETINDEXBUFFER_INVALIDBUFFER",
		"DEVICE_IASETINDEXBUFFER_FORMAT_INVALID",
		"DEVICE_IASETINDEXBUFFER_OFFSET_TOO_LARGE",
		"DEVICE_IASETINDEXBUFFER_OFFSET_UNALIGNED",
		"DEVICE_VSSETSHADERRESOURCES_VIEWS_EMPTY",
		"VSSETCONSTANTBUFFERS_INVALIDBUFFER",
		"DEVICE_VSSETCONSTANTBUFFERS_BUFFERS_EMPTY",
		"DEVICE_VSSETSAMPLERS_SAMPLERS_EMPTY",
		"DEVICE_GSSETSHADERRESOURCES_VIEWS_EMPTY",
		"GSSETCONSTANTBUFFERS_INVALIDBUFFER",
		"DEVICE_GSSETCONSTANTBUFFERS_BUFFERS_EMPTY",
		"DEVICE_GSSETSAMPLERS_SAMPLERS_EMPTY",
		"SOSETTARGETS_INVALIDBUFFER",
		"DEVICE_SOSETTARGETS_OFFSET_UNALIGNED",
		"DEVICE_PSSETSHADERRESOURCES_VIEWS_EMPTY",
		"PSSETCONSTANTBUFFERS_INVALIDBUFFER",
		"DEVICE_PSSETCONSTANTBUFFERS_BUFFERS_EMPTY",
		"DEVICE_PSSETSAMPLERS_SAMPLERS_EMPTY",
		"DEVICE_RSSETVIEWPORTS_INVALIDVIEWPORT",
		"DEVICE_RSSETSCISSORRECTS_INVALIDSCISSOR",
		"CLEARRENDERTARGETVIEW_DENORMFLUSH",
		"CLEARDEPTHSTENCILVIEW_DENORMFLUSH",
		"CLEARDEPTHSTENCILVIEW_INVALID",
		"DEVICE_IAGETVERTEXBUFFERS_BUFFERS_EMPTY",
		"DEVICE_VSGETSHADERRESOURCES_VIEWS_EMPTY",
		"DEVICE_VSGETCONSTANTBUFFERS_BUFFERS_EMPTY",
		"DEVICE_VSGETSAMPLERS_SAMPLERS_EMPTY",
		"DEVICE_GSGETSHADERRESOURCES_VIEWS_EMPTY",
		"DEVICE_GSGETCONSTANTBUFFERS_BUFFERS_EMPTY",
		"DEVICE_GSGETSAMPLERS_SAMPLERS_EMPTY",
		"DEVICE_SOGETTARGETS_BUFFERS_EMPTY",
		"DEVICE_PSGETSHADERRESOURCES_VIEWS_EMPTY",
		"DEVICE_PSGETCONSTANTBUFFERS_BUFFERS_EMPTY",
		"DEVICE_PSGETSAMPLERS_SAMPLERS_EMPTY",
		"DEVICE_RSGETVIEWPORTS_VIEWPORTS_EMPTY",
		"DEVICE_RSGETSCISSORRECTS_RECTS_EMPTY",
		"DEVICE_GENERATEMIPS_RESOURCE_INVALID",
		"COPYSUBRESOURCEREGION_INVALIDDESTINATIONSUBRESOURCE",
		"COPYSUBRESOURCEREGION_INVALIDSOURCESUBRESOURCE",
		"COPYSUBRESOURCEREGION_INVALIDSOURCEBOX",
		"COPYSUBRESOURCEREGION_INVALIDSOURCE",
		"COPYSUBRESOURCEREGION_INVALIDDESTINATIONSTATE",
		"COPYSUBRESOURCEREGION_INVALIDSOURCESTATE",
		"COPYRESOURCE_INVALIDSOURCE",
		"COPYRESOURCE_INVALIDDESTINATIONSTATE",
		"COPYRESOURCE_INVALIDSOURCESTATE",
		"UPDATESUBRESOURCE_INVALIDDESTINATIONSUBRESOURCE",
		"UPDATESUBRESOURCE_INVALIDDESTINATIONBOX",
		"UPDATESUBRESOURCE_INVALIDDESTINATIONSTATE",
		"DEVICE_RESOLVESUBRESOURCE_DESTINATION_INVALID",
		"DEVICE_RESOLVESUBRESOURCE_DESTINATION_SUBRESOURCE_INVALID",
		"DEVICE_RESOLVESUBRESOURCE_SOURCE_INVALID",
		"DEVICE_RESOLVESUBRESOURCE_SOURCE_SUBRESOURCE_INVALID",
		"DEVICE_RESOLVESUBRESOURCE_FORMAT_INVALID",
		"BUFFER_MAP_INVALIDMAPTYPE",
		"BUFFER_MAP_INVALIDFLAGS",
		"BUFFER_MAP_ALREADYMAPPED",
		"BUFFER_MAP_DEVICEREMOVED_RETURN",
		"BUFFER_UNMAP_NOTMAPPED",
		"TEXTURE1D_MAP_INVALIDMAPTYPE",
		"TEXTURE1D_MAP_INVALIDSUBRESOURCE",
		"TEXTURE1D_MAP_INVALIDFLAGS",
		"TEXTURE1D_MAP_ALREADYMAPPED",
		"TEXTURE1D_MAP_DEVICEREMOVED_RETURN",
		"TEXTURE1D_UNMAP_INVALIDSUBRESOURCE",
		"TEXTURE1D_UNMAP_NOTMAPPED",
		"TEXTURE2D_MAP_INVALIDMAPTYPE",
		"TEXTURE2D_MAP_INVALIDSUBRESOURCE",
		"TEXTURE2D_MAP_INVALIDFLAGS",
		"TEXTURE2D_MAP_ALREADYMAPPED",
		"TEXTURE2D_MAP_DEVICEREMOVED_RETURN",
		"TEXTURE2D_UNMAP_INVALIDSUBRESOURCE",
		"TEXTURE2D_UNMAP_NOTMAPPED",
		"TEXTURE3D_MAP_INVALIDMAPTYPE",
		"TEXTURE3D_MAP_INVALIDSUBRESOURCE",
		"TEXTURE3D_MAP_INVALIDFLAGS",
		"TEXTURE3D_MAP_ALREADYMAPPED",
		"TEXTURE3D_MAP_DEVICEREMOVED_RETURN",
		"TEXTURE3D_UNMAP_INVALIDSUBRESOURCE",
		"TEXTURE3D_UNMAP_NOTMAPPED",
		"CHECKFORMATSUPPORT_FORMAT_DEPRECATED",
		"CHECKMULTISAMPLEQUALITYLEVELS_FORMAT_DEPRECATED",
		"SETEXCEPTIONMODE_UNRECOGNIZEDFLAGS",
		"SETEXCEPTIONMODE_INVALIDARG_RETURN",
		"SETEXCEPTIONMODE_DEVICEREMOVED_RETURN",
		"REF_SIMULATING_INFINITELY_FAST_HARDWARE",
		"REF_THREADING_MODE",
		"REF_UMDRIVER_EXCEPTION",
		"REF_KMDRIVER_EXCEPTION",
		"REF_HARDWARE_EXCEPTION",
		"REF_ACCESSING_INDEXABLE_TEMP_OUT_OF_RANGE",
		"REF_PROBLEM_PARSING_SHADER",
		"REF_OUT_OF_MEMORY",
		"REF_INFO",
		"DEVICE_DRAW_VERTEXPOS_OVERFLOW",
		"DEVICE_DRAWINDEXED_INDEXPOS_OVERFLOW",
		"DEVICE_DRAWINSTANCED_VERTEXPOS_OVERFLOW",
		"DEVICE_DRAWINSTANCED_INSTANCEPOS_OVERFLOW",
		"DEVICE_DRAWINDEXEDINSTANCED_INSTANCEPOS_OVERFLOW",
		"DEVICE_DRAWINDEXEDINSTANCED_INDEXPOS_OVERFLOW",
		"DEVICE_DRAW_VERTEX_SHADER_NOT_SET",
		"DEVICE_SHADER_LINKAGE_SEMANTICNAME_NOT_FOUND",
		"DEVICE_SHADER_LINKAGE_REGISTERINDEX",
		"DEVICE_SHADER_LINKAGE_COMPONENTTYPE",
		"DEVICE_SHADER_LINKAGE_REGISTERMASK",
		"DEVICE_SHADER_LINKAGE_SYSTEMVALUE",
		"DEVICE_SHADER_LINKAGE_NEVERWRITTEN_ALWAYSREADS",
		"DEVICE_DRAW_VERTEX_BUFFER_NOT_SET",
		"DEVICE_DRAW_INPUTLAYOUT_NOT_SET",
		"DEVICE_DRAW_CONSTANT_BUFFER_NOT_SET",
		"DEVICE_DRAW_CONSTANT_BUFFER_TOO_SMALL",
		"DEVICE_DRAW_SAMPLER_NOT_SET",
		"DEVICE_DRAW_SHADERRESOURCEVIEW_NOT_SET",
		"DEVICE_DRAW_VIEW_DIMENSION_MISMATCH",
		"DEVICE_DRAW_VERTEX_BUFFER_STRIDE_TOO_SMALL",
		"DEVICE_DRAW_VERTEX_BUFFER_TOO_SMALL",
		"DEVICE_DRAW_INDEX_BUFFER_NOT_SET",
		"DEVICE_DRAW_INDEX_BUFFER_FORMAT_INVALID",
		"DEVICE_DRAW_INDEX_BUFFER_TOO_SMALL",
		"DEVICE_DRAW_GS_INPUT_PRIMITIVE_MISMATCH",
		"DEVICE_DRAW_RESOURCE_RETURN_TYPE_MISMATCH",
		"DEVICE_DRAW_POSITION_NOT_PRESENT",
		"DEVICE_DRAW_OUTPUT_STREAM_NOT_SET",
		"DEVICE_DRAW_BOUND_RESOURCE_MAPPED",
		"DEVICE_DRAW_INVALID_PRIMITIVETOPOLOGY",
		"DEVICE_DRAW_VERTEX_OFFSET_UNALIGNED",
		"DEVICE_DRAW_VERTEX_STRIDE_UNALIGNED",
		"DEVICE_DRAW_INDEX_OFFSET_UNALIGNED",
		"DEVICE_DRAW_OUTPUT_STREAM_OFFSET_UNALIGNED",
		"DEVICE_DRAW_RESOURCE_FORMAT_LD_UNSUPPORTED",
		"DEVICE_DRAW_RESOURCE_FORMAT_SAMPLE_UNSUPPORTED",
		"DEVICE_DRAW_RESOURCE_FORMAT_SAMPLE_C_UNSUPPORTED",
		"DEVICE_DRAW_RESOURCE_MULTISAMPLE_UNSUPPORTED",
		"DEVICE_DRAW_SO_TARGETS_BOUND_WITHOUT_SOURCE",
		"DEVICE_DRAW_SO_STRIDE_LARGER_THAN_BUFFER",
		"DEVICE_DRAW_OM_RENDER_TARGET_DOES_NOT_SUPPORT_BLENDING",
		"DEVICE_DRAW_OM_DUAL_SOURCE_BLENDING_CAN_ONLY_HAVE_RENDER_TARGET_0",
		"DEVICE_REMOVAL_PROCESS_AT_FAULT",
		"DEVICE_REMOVAL_PROCESS_POSSIBLY_AT_FAULT",
		"DEVICE_REMOVAL_PROCESS_NOT_AT_FAULT",
		"DEVICE_OPEN_SHARED_RESOURCE_INVALIDARG_RETURN",
		"DEVICE_OPEN_SHARED_RESOURCE_OUTOFMEMORY_RETURN",
		"DEVICE_OPEN_SHARED_RESOURCE_BADINTERFACE_RETURN",
		"DEVICE_DRAW_VIEWPORT_NOT_SET",
		"CREATEINPUTLAYOUT_TRAILING_DIGIT_IN_SEMANTIC",
		"CREATEGEOMETRYSHADERWITHSTREAMOUTPUT_TRAILING_DIGIT_IN_SEMANTIC",
		"DEVICE_RSSETVIEWPORTS_DENORMFLUSH",
		"OMSETRENDERTARGETS_INVALIDVIEW",
		"DEVICE_SETTEXTFILTERSIZE_INVALIDDIMENSIONS",
		"DEVICE_DRAW_SAMPLER_MISMATCH",
		"CREATEINPUTLAYOUT_TYPE_MISMATCH",
		"BLENDSTATE_GETDESC_LEGACY",
		"SHADERRESOURCEVIEW_GETDESC_LEGACY",
		"CREATEQUERY_OUTOFMEMORY_RETURN",
		"CREATEPREDICATE_OUTOFMEMORY_RETURN",
		"CREATECOUNTER_OUTOFRANGE_COUNTER",
		"CREATECOUNTER_SIMULTANEOUS_ACTIVE_COUNTERS_EXHAUSTED",
		"CREATECOUNTER_UNSUPPORTED_WELLKNOWN_COUNTER",
		"CREATECOUNTER_OUTOFMEMORY_RETURN",
		"CREATECOUNTER_NONEXCLUSIVE_RETURN",
		"CREATECOUNTER_NULLDESC",
		"CHECKCOUNTER_OUTOFRANGE_COUNTER",
		"CHECKCOUNTER_UNSUPPORTED_WELLKNOWN_COUNTER",
		"SETPREDICATION_INVALID_PREDICATE_STATE",
		"QUERY_BEGIN_UNSUPPORTED",
		"PREDICATE_BEGIN_DURING_PREDICATION",
		"QUERY_BEGIN_DUPLICATE",
		"QUERY_BEGIN_ABANDONING_PREVIOUS_RESULTS",
		"PREDICATE_END_DURING_PREDICATION",
		"QUERY_END_ABANDONING_PREVIOUS_RESULTS",
		"QUERY_END_WITHOUT_BEGIN",
		"QUERY_GETDATA_INVALID_DATASIZE",
		"QUERY_GETDATA_INVALID_FLAGS",
		"QUERY_GETDATA_INVALID_CALL",
		"DEVICE_DRAW_PS_OUTPUT_TYPE_MISMATCH",
		"DEVICE_DRAW_RESOURCE_FORMAT_GATHER_UNSUPPORTED",
		"DEVICE_DRAW_INVALID_USE_OF_CENTER_MULTISAMPLE_PATTERN",
		"DEVICE_IASETVERTEXBUFFERS_STRIDE_TOO_LARGE",
		"DEVICE_IASETVERTEXBUFFERS_INVALIDRANGE",
		"CREATEINPUTLAYOUT_EMPTY_LAYOUT",
		"DEVICE_DRAW_RESOURCE_SAMPLE_COUNT_MISMATCH",
		"LIVE_OBJECT_SUMMARY",
		"LIVE_BUFFER",
		"LIVE_TEXTURE1D",
		"LIVE_TEXTURE2D",
		"LIVE_TEXTURE3D",
		"LIVE_SHADERRESOURCEVIEW",
		"LIVE_RENDERTARGETVIEW",
		"LIVE_DEPTHSTENCILVIEW",
		"LIVE_VERTEXSHADER",
		"LIVE_GEOMETRYSHADER",
		"LIVE_PIXELSHADER",
		"LIVE_INPUTLAYOUT",
		"LIVE_SAMPLER",
		"LIVE_BLENDSTATE",
		"LIVE_DEPTHSTENCILSTATE",
		"LIVE_RASTERIZERSTATE",
		"LIVE_QUERY",
		"LIVE_PREDICATE",
		"LIVE_COUNTER",
		"LIVE_DEVICE",
		"LIVE_SWAPCHAIN",
		"D3D10_MESSAGES_END",
		"D3D10L9_MESSAGES_START",
		"CREATERASTERIZERSTATE_DepthBiasClamp_NOT_SUPPORTED",
		"CREATESAMPLERSTATE_NO_COMPARISON_SUPPORT",
		"CREATESAMPLERSTATE_EXCESSIVE_ANISOTROPY",
		"CREATESAMPLERSTATE_BORDER_OUT_OF_RANGE",
		"VSSETSAMPLERS_NOT_SUPPORTED",
		"VSSETSAMPLERS_TOO_MANY_SAMPLERS",
		"PSSETSAMPLERS_TOO_MANY_SAMPLERS",
		"CREATERESOURCE_NO_ARRAYS",
		"CREATERESOURCE_NO_VB_AND_IB_BIND",
		"CREATERESOURCE_NO_TEXTURE_1D",
		"CREATERESOURCE_DIMENSION_OUT_OF_RANGE",
		"CREATERESOURCE_NOT_BINDABLE_AS_SHADER_RESOURCE",
		"OMSETRENDERTARGETS_TOO_MANY_RENDER_TARGETS",
		"OMSETRENDERTARGETS_NO_DIFFERING_BIT_DEPTHS",
		"IASETVERTEXBUFFERS_BAD_BUFFER_INDEX",
		"DEVICE_RSSETVIEWPORTS_TOO_MANY_VIEWPORTS",
		"DEVICE_IASETPRIMITIVETOPOLOGY_ADJACENCY_UNSUPPORTED",
		"DEVICE_RSSETSCISSORRECTS_TOO_MANY_SCISSORS",
		"COPYRESOURCE_ONLY_TEXTURE_2D_WITHIN_GPU_MEMORY",
		"COPYRESOURCE_NO_TEXTURE_3D_READBACK",
		"COPYRESOURCE_NO_TEXTURE_ONLY_READBACK",
		"CREATEINPUTLAYOUT_UNSUPPORTED_FORMAT",
		"CREATEBLENDSTATE_NO_ALPHA_TO_COVERAGE",
		"CREATERASTERIZERSTATE_DepthClipEnable_MUST_BE_TRUE",
		"DRAWINDEXED_STARTINDEXLOCATION_MUST_BE_POSITIVE",
		"CREATESHADERRESOURCEVIEW_MUST_USE_LOWEST_LOD",
		"CREATESAMPLERSTATE_MINLOD_MUST_NOT_BE_FRACTIONAL",
		"CREATESAMPLERSTATE_MAXLOD_MUST_BE_FLT_MAX",
		"CREATESHADERRESOURCEVIEW_FIRSTARRAYSLICE_MUST_BE_ZERO",
		"CREATESHADERRESOURCEVIEW_CUBES_MUST_HAVE_6_SIDES",
		"CREATERESOURCE_NOT_BINDABLE_AS_RENDER_TARGET",
		"CREATERESOURCE_NO_DWORD_INDEX_BUFFER",
		"CREATERESOURCE_MSAA_PRECLUDES_SHADER_RESOURCE",
		"CREATERESOURCE_PRESENTATION_PRECLUDES_SHADER_RESOURCE",
		"CREATEBLENDSTATE_NO_INDEPENDENT_BLEND_ENABLE",
		"CREATEBLENDSTATE_NO_INDEPENDENT_WRITE_MASKS",
		"CREATERESOURCE_NO_STREAM_OUT",
		"CREATERESOURCE_ONLY_VB_IB_FOR_BUFFERS",
		"CREATERESOURCE_NO_AUTOGEN_FOR_VOLUMES",
		"CREATERESOURCE_DXGI_FORMAT_R8G8B8A8_CANNOT_BE_SHARED",
		"VSSHADERRESOURCES_NOT_SUPPORTED",
		"GEOMETRY_SHADER_NOT_SUPPORTED",
		"STREAM_OUT_NOT_SUPPORTED",
		"TEXT_FILTER_NOT_SUPPORTED",
		"CREATEBLENDSTATE_NO_SEPARATE_ALPHA_BLEND",
		"CREATEBLENDSTATE_NO_MRT_BLEND",
		"CREATEBLENDSTATE_OPERATION_NOT_SUPPORTED",
		"CREATESAMPLERSTATE_NO_MIRRORONCE",
		"DRAWINSTANCED_NOT_SUPPORTED",
		"DRAWINDEXEDINSTANCED_NOT_SUPPORTED_BELOW_9_3",
		"DRAWINDEXED_POINTLIST_UNSUPPORTED",
		"SETBLENDSTATE_SAMPLE_MASK_CANNOT_BE_ZERO",
		"CREATERESOURCE_DIMENSION_EXCEEDS_FEATURE_LEVEL_DEFINITION",
		"CREATERESOURCE_ONLY_SINGLE_MIP_LEVEL_DEPTH_STENCIL_SUPPORTED",
		"DEVICE_RSSETSCISSORRECTS_NEGATIVESCISSOR",
		"SLOT_ZERO_MUST_BE_D3D10_INPUT_PER_VERTEX_DATA",
		"CREATERESOURCE_NON_POW_2_MIPMAP",
		"CREATESAMPLERSTATE_BORDER_NOT_SUPPORTED",
		"OMSETRENDERTARGETS_NO_SRGB_MRT",
		"COPYRESOURCE_NO_3D_MISMATCHED_UPDATES",
		"D3D10L9_MESSAGES_END",
		"D3D11_MESSAGES_START",
		"CREATEVERTEXSHADER_INVALIDCLASSLINKAGE",
		"CREATEGEOMETRYSHADER_INVALIDCLASSLINKAGE",
		"CREATEGEOMETRYSHADERWITHSTREAMOUTPUT_INVALIDNUMSTREAMS",
		"CREATEGEOMETRYSHADERWITHSTREAMOUTPUT_INVALIDSTREAMTORASTERIZER",
		"CREATEGEOMETRYSHADERWITHSTREAMOUTPUT_UNEXPECTEDSTREAMS",
		"CREATEGEOMETRYSHADERWITHSTREAMOUTPUT_INVALIDCLASSLINKAGE",
		"CREATEPIXELSHADER_INVALIDCLASSLINKAGE",
		"CREATEDEFERREDCONTEXT_INVALID_COMMANDLISTFLAGS",
		"CREATEDEFERREDCONTEXT_SINGLETHREADED",
		"CREATEDEFERREDCONTEXT_INVALIDARG_RETURN",
		"CREATEDEFERREDCONTEXT_INVALID_CALL_RETURN",
		"CREATEDEFERREDCONTEXT_OUTOFMEMORY_RETURN",
		"FINISHDISPLAYLIST_ONIMMEDIATECONTEXT",
		"FINISHDISPLAYLIST_OUTOFMEMORY_RETURN",
		"FINISHDISPLAYLIST_INVALID_CALL_RETURN",
		"CREATEGEOMETRYSHADERWITHSTREAMOUTPUT_INVALIDSTREAM",
		"CREATEGEOMETRYSHADERWITHSTREAMOUTPUT_UNEXPECTEDENTRIES",
		"CREATEGEOMETRYSHADERWITHSTREAMOUTPUT_UNEXPECTEDSTRIDES",
		"CREATEGEOMETRYSHADERWITHSTREAMOUTPUT_INVALIDNUMSTRIDES",
		"DEVICE_HSSETSHADERRESOURCES_HAZARD",
		"DEVICE_HSSETCONSTANTBUFFERS_HAZARD",
		"HSSETSHADERRESOURCES_UNBINDDELETINGOBJECT",
		"HSSETCONSTANTBUFFERS_UNBINDDELETINGOBJECT",
		"CREATEHULLSHADER_INVALIDCALL",
		"CREATEHULLSHADER_OUTOFMEMORY",
		"CREATEHULLSHADER_INVALIDSHADERBYTECODE",
		"CREATEHULLSHADER_INVALIDSHADERTYPE",
		"CREATEHULLSHADER_INVALIDCLASSLINKAGE",
		"DEVICE_HSSETSHADERRESOURCES_VIEWS_EMPTY",
		"HSSETCONSTANTBUFFERS_INVALIDBUFFER",
		"DEVICE_HSSETCONSTANTBUFFERS_BUFFERS_EMPTY",
		"DEVICE_HSSETSAMPLERS_SAMPLERS_EMPTY",
		"DEVICE_HSGETSHADERRESOURCES_VIEWS_EMPTY",
		"DEVICE_HSGETCONSTANTBUFFERS_BUFFERS_EMPTY",
		"DEVICE_HSGETSAMPLERS_SAMPLERS_EMPTY",
		"DEVICE_DSSETSHADERRESOURCES_HAZARD",
		"DEVICE_DSSETCONSTANTBUFFERS_HAZARD",
		"DSSETSHADERRESOURCES_UNBINDDELETINGOBJECT",
		"DSSETCONSTANTBUFFERS_UNBINDDELETINGOBJECT",
		"CREATEDOMAINSHADER_INVALIDCALL",
		"CREATEDOMAINSHADER_OUTOFMEMORY",
		"CREATEDOMAINSHADER_INVALIDSHADERBYTECODE",
		"CREATEDOMAINSHADER_INVALIDSHADERTYPE",
		"CREATEDOMAINSHADER_INVALIDCLASSLINKAGE",
		"DEVICE_DSSETSHADERRESOURCES_VIEWS_EMPTY",
		"DSSETCONSTANTBUFFERS_INVALIDBUFFER",
		"DEVICE_DSSETCONSTANTBUFFERS_BUFFERS_EMPTY",
		"DEVICE_DSSETSAMPLERS_SAMPLERS_EMPTY",
		"DEVICE_DSGETSHADERRESOURCES_VIEWS_EMPTY",
		"DEVICE_DSGETCONSTANTBUFFERS_BUFFERS_EMPTY",
		"DEVICE_DSGETSAMPLERS_SAMPLERS_EMPTY",
		"DEVICE_DRAW_HS_XOR_DS_MISMATCH",
		"DEFERRED_CONTEXT_REMOVAL_PROCESS_AT_FAULT",
		"DEVICE_DRAWINDIRECT_INVALID_ARG_BUFFER",
		"DEVICE_DRAWINDIRECT_OFFSET_UNALIGNED",
		"DEVICE_DRAWINDIRECT_OFFSET_OVERFLOW",
		"RESOURCE_MAP_INVALIDMAPTYPE",
		"RESOURCE_MAP_INVALIDSUBRESOURCE",
		"RESOURCE_MAP_INVALIDFLAGS",
		"RESOURCE_MAP_ALREADYMAPPED",
		"RESOURCE_MAP_DEVICEREMOVED_RETURN",
		"RESOURCE_MAP_OUTOFMEMORY_RETURN",
		"RESOURCE_MAP_WITHOUT_INITIAL_DISCARD",
		"RESOURCE_UNMAP_INVALIDSUBRESOURCE",
		"RESOURCE_UNMAP_NOTMAPPED",
		"DEVICE_DRAW_RASTERIZING_CONTROL_POINTS",
		"DEVICE_IASETPRIMITIVETOPOLOGY_TOPOLOGY_UNSUPPORTED",
		"DEVICE_DRAW_HS_DS_SIGNATURE_MISMATCH",
		"DEVICE_DRAW_HULL_SHADER_INPUT_TOPOLOGY_MISMATCH",
		"DEVICE_DRAW_HS_DS_CONTROL_POINT_COUNT_MISMATCH",
		"DEVICE_DRAW_HS_DS_TESSELLATOR_DOMAIN_MISMATCH",
		"CREATE_CONTEXT",
		"LIVE_CONTEXT",
		"DESTROY_CONTEXT",
		"CREATE_BUFFER",
		"LIVE_BUFFER_WIN7",
		"DESTROY_BUFFER",
		"CREATE_TEXTURE1D",
		"LIVE_TEXTURE1D_WIN7",
		"DESTROY_TEXTURE1D",
		"CREATE_TEXTURE2D",
		"LIVE_TEXTURE2D_WIN7",
		"DESTROY_TEXTURE2D",
		"CREATE_TEXTURE3D",
		"LIVE_TEXTURE3D_WIN7",
		"DESTROY_TEXTURE3D",
		"CREATE_SHADERRESOURCEVIEW",
		"LIVE_SHADERRESOURCEVIEW_WIN7",
		"DESTROY_SHADERRESOURCEVIEW",
		"CREATE_RENDERTARGETVIEW",
		"LIVE_RENDERTARGETVIEW_WIN7",
		"DESTROY_RENDERTARGETVIEW",
		"CREATE_DEPTHSTENCILVIEW",
		"LIVE_DEPTHSTENCILVIEW_WIN7",
		"DESTROY_DEPTHSTENCILVIEW",
		"CREATE_VERTEXSHADER",
		"LIVE_VERTEXSHADER_WIN7",
		"DESTROY_VERTEXSHADER",
		"CREATE_HULLSHADER",
		"LIVE_HULLSHADER",
		"DESTROY_HULLSHADER",
		"CREATE_DOMAINSHADER",
		"LIVE_DOMAINSHADER",
		"DESTROY_DOMAINSHADER",
		"CREATE_GEOMETRYSHADER",
		"LIVE_GEOMETRYSHADER_WIN7",
		"DESTROY_GEOMETRYSHADER",
		"CREATE_PIXELSHADER",
		"LIVE_PIXELSHADER_WIN7",
		"DESTROY_PIXELSHADER",
		"CREATE_INPUTLAYOUT",
		"LIVE_INPUTLAYOUT_WIN7",
		"DESTROY_INPUTLAYOUT",
		"CREATE_SAMPLER",
		"LIVE_SAMPLER_WIN7",
		"DESTROY_SAMPLER",
		"CREATE_BLENDSTATE",
		"LIVE_BLENDSTATE_WIN7",
		"DESTROY_BLENDSTATE",
		"CREATE_DEPTHSTENCILSTATE",
		"LIVE_DEPTHSTENCILSTATE_WIN7",
		"DESTROY_DEPTHSTENCILSTATE",
		"CREATE_RASTERIZERSTATE",
		"LIVE_RASTERIZERSTATE_WIN7",
		"DESTROY_RASTERIZERSTATE",
		"CREATE_QUERY",
		"LIVE_QUERY_WIN7",
		"DESTROY_QUERY",
		"CREATE_PREDICATE",
		"LIVE_PREDICATE_WIN7",
		"DESTROY_PREDICATE",
		"CREATE_COUNTER",
		"DESTROY_COUNTER",
		"CREATE_COMMANDLIST",
		"LIVE_COMMANDLIST",
		"DESTROY_COMMANDLIST",
		"CREATE_CLASSINSTANCE",
		"LIVE_CLASSINSTANCE",
		"DESTROY_CLASSINSTANCE",
		"CREATE_CLASSLINKAGE",
		"LIVE_CLASSLINKAGE",
		"DESTROY_CLASSLINKAGE",
		"LIVE_DEVICE_WIN7",
		"LIVE_OBJECT_SUMMARY_WIN7",
		"CREATE_COMPUTESHADER",
		"LIVE_COMPUTESHADER",
		"DESTROY_COMPUTESHADER",
		"CREATE_UNORDEREDACCESSVIEW",
		"LIVE_UNORDEREDACCESSVIEW",
		"DESTROY_UNORDEREDACCESSVIEW",
		"DEVICE_SETSHADER_INTERFACES_FEATURELEVEL",
		"DEVICE_SETSHADER_INTERFACE_COUNT_MISMATCH",
		"DEVICE_SETSHADER_INVALID_INSTANCE",
		"DEVICE_SETSHADER_INVALID_INSTANCE_INDEX",
		"DEVICE_SETSHADER_INVALID_INSTANCE_TYPE",
		"DEVICE_SETSHADER_INVALID_INSTANCE_DATA",
		"DEVICE_SETSHADER_UNBOUND_INSTANCE_DATA",
		"DEVICE_SETSHADER_INSTANCE_DATA_BINDINGS",
		"DEVICE_CREATESHADER_CLASSLINKAGE_FULL",
		"DEVICE_CHECKFEATURESUPPORT_UNRECOGNIZED_FEATURE",
		"DEVICE_CHECKFEATURESUPPORT_MISMATCHED_DATA_SIZE",
		"DEVICE_CHECKFEATURESUPPORT_INVALIDARG_RETURN",
		"DEVICE_CSSETSHADERRESOURCES_HAZARD",
		"DEVICE_CSSETCONSTANTBUFFERS_HAZARD",
		"CSSETSHADERRESOURCES_UNBINDDELETINGOBJECT",
		"CSSETCONSTANTBUFFERS_UNBINDDELETINGOBJECT",
		"CREATECOMPUTESHADER_INVALIDCALL",
		"CREATECOMPUTESHADER_OUTOFMEMORY",
		"CREATECOMPUTESHADER_INVALIDSHADERBYTECODE",
		"CREATECOMPUTESHADER_INVALIDSHADERTYPE",
		"CREATECOMPUTESHADER_INVALIDCLASSLINKAGE",
		"DEVICE_CSSETSHADERRESOURCES_VIEWS_EMPTY",
		"CSSETCONSTANTBUFFERS_INVALIDBUFFER",
		"DEVICE_CSSETCONSTANTBUFFERS_BUFFERS_EMPTY",
		"DEVICE_CSSETSAMPLERS_SAMPLERS_EMPTY",
		"DEVICE_CSGETSHADERRESOURCES_VIEWS_EMPTY",
		"DEVICE_CSGETCONSTANTBUFFERS_BUFFERS_EMPTY",
		"DEVICE_CSGETSAMPLERS_SAMPLERS_EMPTY",
		"DEVICE_CREATEVERTEXSHADER_DOUBLEFLOATOPSNOTSUPPORTED",
		"DEVICE_CREATEHULLSHADER_DOUBLEFLOATOPSNOTSUPPORTED",
		"DEVICE_CREATEDOMAINSHADER_DOUBLEFLOATOPSNOTSUPPORTED",
		"DEVICE_CREATEGEOMETRYSHADER_DOUBLEFLOATOPSNOTSUPPORTED",
		"DEVICE_CREATEGEOMETRYSHADERWITHSTREAMOUTPUT_DOUBLEFLOATOPSNOTSUPPORTED",
		"DEVICE_CREATEPIXELSHADER_DOUBLEFLOATOPSNOTSUPPORTED",
		"DEVICE_CREATECOMPUTESHADER_DOUBLEFLOATOPSNOTSUPPORTED",
		"CREATEBUFFER_INVALIDSTRUCTURESTRIDE",
		"CREATESHADERRESOURCEVIEW_INVALIDFLAGS",
		"CREATEUNORDEREDACCESSVIEW_INVALIDRESOURCE",
		"CREATEUNORDEREDACCESSVIEW_INVALIDDESC",
		"CREATEUNORDEREDACCESSVIEW_INVALIDFORMAT",
		"CREATEUNORDEREDACCESSVIEW_INVALIDDIMENSIONS",
		"CREATEUNORDEREDACCESSVIEW_UNRECOGNIZEDFORMAT",
		"DEVICE_OMSETRENDERTARGETSANDUNORDEREDACCESSVIEWS_HAZARD",
		"DEVICE_OMSETRENDERTARGETSANDUNORDEREDACCESSVIEWS_OVERLAPPING_OLD_SLOTS",
		"DEVICE_OMSETRENDERTARGETSANDUNORDEREDACCESSVIEWS_NO_OP",
		"CSSETUNORDEREDACCESSVIEWS_UNBINDDELETINGOBJECT",
		"PSSETUNORDEREDACCESSVIEWS_UNBINDDELETINGOBJECT",
		"CREATEUNORDEREDACCESSVIEW_INVALIDARG_RETURN",
		"CREATEUNORDEREDACCESSVIEW_OUTOFMEMORY_RETURN",
		"CREATEUNORDEREDACCESSVIEW_TOOMANYOBJECTS",
		"DEVICE_CSSETUNORDEREDACCESSVIEWS_HAZARD",
		"CLEARUNORDEREDACCESSVIEW_DENORMFLUSH",
		"DEVICE_CSSETUNORDEREDACCESSS_VIEWS_EMPTY",
		"DEVICE_CSGETUNORDEREDACCESSS_VIEWS_EMPTY",
		"CREATEUNORDEREDACCESSVIEW_INVALIDFLAGS",
		"CREATESHADERRESESOURCEVIEW_TOOMANYOBJECTS",
		"DEVICE_DISPATCHINDIRECT_INVALID_ARG_BUFFER",
		"DEVICE_DISPATCHINDIRECT_OFFSET_UNALIGNED",
		"DEVICE_DISPATCHINDIRECT_OFFSET_OVERFLOW",
		"DEVICE_SETRESOURCEMINLOD_INVALIDCONTEXT",
		"DEVICE_SETRESOURCEMINLOD_INVALIDRESOURCE",
		"DEVICE_SETRESOURCEMINLOD_INVALIDMINLOD",
		"DEVICE_GETRESOURCEMINLOD_INVALIDCONTEXT",
		"DEVICE_GETRESOURCEMINLOD_INVALIDRESOURCE",
		"OMSETDEPTHSTENCIL_UNBINDDELETINGOBJECT",
		"CLEARDEPTHSTENCILVIEW_DEPTH_READONLY",
		"CLEARDEPTHSTENCILVIEW_STENCIL_READONLY",
		"CHECKFEATURESUPPORT_FORMAT_DEPRECATED",
		"DEVICE_UNORDEREDACCESSVIEW_RETURN_TYPE_MISMATCH",
		"DEVICE_UNORDEREDACCESSVIEW_NOT_SET",
		"DEVICE_DRAW_UNORDEREDACCESSVIEW_RENDERTARGETVIEW_OVERLAP",
		"DEVICE_UNORDEREDACCESSVIEW_DIMENSION_MISMATCH",
		"DEVICE_UNORDEREDACCESSVIEW_APPEND_UNSUPPORTED",
		"DEVICE_UNORDEREDACCESSVIEW_ATOMICS_UNSUPPORTED",
		"DEVICE_UNORDEREDACCESSVIEW_STRUCTURE_STRIDE_MISMATCH",
		"DEVICE_UNORDEREDACCESSVIEW_BUFFER_TYPE_MISMATCH",
		"DEVICE_UNORDEREDACCESSVIEW_RAW_UNSUPPORTED",
		"DEVICE_UNORDEREDACCESSVIEW_FORMAT_LD_UNSUPPORTED",
		"DEVICE_UNORDEREDACCESSVIEW_FORMAT_STORE_UNSUPPORTED",
		"DEVICE_UNORDEREDACCESSVIEW_ATOMIC_ADD_UNSUPPORTED",
		"DEVICE_UNORDEREDACCESSVIEW_ATOMIC_BITWISE_OPS_UNSUPPORTED",
		"DEVICE_UNORDEREDACCESSVIEW_ATOMIC_CMPSTORE_CMPEXCHANGE_UNSUPPORTED",
		"DEVICE_UNORDEREDACCESSVIEW_ATOMIC_EXCHANGE_UNSUPPORTED",
		"DEVICE_UNORDEREDACCESSVIEW_ATOMIC_SIGNED_MINMAX_UNSUPPORTED",
		"DEVICE_UNORDEREDACCESSVIEW_ATOMIC_UNSIGNED_MINMAX_UNSUPPORTED",
		"DEVICE_DISPATCH_BOUND_RESOURCE_MAPPED",
		"DEVICE_DISPATCH_THREADGROUPCOUNT_OVERFLOW",
		"DEVICE_DISPATCH_THREADGROUPCOUNT_ZERO",
		"DEVICE_SHADERRESOURCEVIEW_STRUCTURE_STRIDE_MISMATCH",
		"DEVICE_SHADERRESOURCEVIEW_BUFFER_TYPE_MISMATCH",
		"DEVICE_SHADERRESOURCEVIEW_RAW_UNSUPPORTED",
		"DEVICE_DISPATCH_UNSUPPORTED",
		"DEVICE_DISPATCHINDIRECT_UNSUPPORTED",
		"COPYSTRUCTURECOUNT_INVALIDOFFSET",
		"COPYSTRUCTURECOUNT_LARGEOFFSET",
		"COPYSTRUCTURECOUNT_INVALIDDESTINATIONSTATE",
		"COPYSTRUCTURECOUNT_INVALIDSOURCESTATE",
		"CHECKFORMATSUPPORT_FORMAT_NOT_SUPPORTED",
		"DEVICE_CSSETUNORDEREDACCESSVIEWS_INVALIDVIEW",
		"DEVICE_CSSETUNORDEREDACCESSVIEWS_INVALIDOFFSET",
		"DEVICE_CSSETUNORDEREDACCESSVIEWS_TOOMANYVIEWS",
		"CLEARUNORDEREDACCESSVIEWFLOAT_INVALIDFORMAT",
		"DEVICE_UNORDEREDACCESSVIEW_COUNTER_UNSUPPORTED",
		"REF_WARNING",
		"DEVICE_DRAW_PIXEL_SHADER_WITHOUT_RTV_OR_DSV",
		"SHADER_ABORT",
		"SHADER_MESSAGE",
		"SHADER_ERROR",
		"OFFERRESOURCES_INVALIDRESOURCE",
		"HSSETSAMPLERS_UNBINDDELETINGOBJECT",
		"DSSETSAMPLERS_UNBINDDELETINGOBJECT",
		"CSSETSAMPLERS_UNBINDDELETINGOBJECT",
		"HSSETSHADER_UNBINDDELETINGOBJECT",
		"DSSETSHADER_UNBINDDELETINGOBJECT",
		"CSSETSHADER_UNBINDDELETINGOBJECT",
		"ENQUEUESETEVENT_INVALIDARG_RETURN",
		"ENQUEUESETEVENT_OUTOFMEMORY_RETURN",
		"ENQUEUESETEVENT_ACCESSDENIED_RETURN",
		"DEVICE_OMSETRENDERTARGETSANDUNORDEREDACCESSVIEWS_NUMUAVS_INVALIDRANGE",
		"USE_OF_ZERO_REFCOUNT_OBJECT",
		"D3D11_MESSAGES_END",
		"D3D11_1_MESSAGES_START",
		"CREATE_VIDEOPROCESSORENUM",
		"CREATE_VIDEOPROCESSOR",
		"CREATE_DECODEROUTPUTVIEW",
		"CREATE_PROCESSORINPUTVIEW",
		"CREATE_PROCESSOROUTPUTVIEW",
		"CREATE_DEVICECONTEXTSTATE",
		"LIVE_VIDEODECODER",
		"LIVE_VIDEOPROCESSORENUM",
		"LIVE_VIDEOPROCESSOR",
		"LIVE_DECODEROUTPUTVIEW",
		"LIVE_PROCESSORINPUTVIEW",
		"LIVE_PROCESSOROUTPUTVIEW",
		"LIVE_DEVICECONTEXTSTATE",
		"DESTROY_VIDEODECODER",
		"DESTROY_VIDEOPROCESSORENUM",
		"DESTROY_VIDEOPROCESSOR",
		"DESTROY_DECODEROUTPUTVIEW",
		"DESTROY_PROCESSORINPUTVIEW",
		"DESTROY_PROCESSOROUTPUTVIEW",
		"DESTROY_DEVICECONTEXTSTATE",
		"CREATEDEVICECONTEXTSTATE_INVALIDFLAGS",
		"CREATEDEVICECONTEXTSTATE_INVALIDFEATURELEVEL",
		"CREATEDEVICECONTEXTSTATE_FEATURELEVELS_NOT_SUPPORTED",
		"CREATEDEVICECONTEXTSTATE_INVALIDREFIID",
		"DEVICE_DISCARDVIEW_INVALIDVIEW",
		"COPYSUBRESOURCEREGION1_INVALIDCOPYFLAGS",
		"UPDATESUBRESOURCE1_INVALIDCOPYFLAGS",
		"CREATERASTERIZERSTATE_INVALIDFORCEDSAMPLECOUNT",
		"CREATEVIDEODECODER_OUTOFMEMORY_RETURN",
		"CREATEVIDEODECODER_NULLPARAM",
		"CREATEVIDEODECODER_INVALIDFORMAT",
		"CREATEVIDEODECODER_ZEROWIDTHHEIGHT",
		"CREATEVIDEODECODER_DRIVER_INVALIDBUFFERSIZE",
		"CREATEVIDEODECODER_DRIVER_INVALIDBUFFERUSAGE",
		"GETVIDEODECODERPROFILECOUNT_OUTOFMEMORY",
		"GETVIDEODECODERPROFILE_NULLPARAM",
		"GETVIDEODECODERPROFILE_INVALIDINDEX",
		"GETVIDEODECODERPROFILE_OUTOFMEMORY_RETURN",
		"CHECKVIDEODECODERFORMAT_NULLPARAM",
		"CHECKVIDEODECODERFORMAT_OUTOFMEMORY_RETURN",
		"GETVIDEODECODERCONFIGCOUNT_NULLPARAM",
		"GETVIDEODECODERCONFIGCOUNT_OUTOFMEMORY_RETURN",
		"GETVIDEODECODERCONFIG_NULLPARAM",
		"GETVIDEODECODERCONFIG_INVALIDINDEX",
		"GETVIDEODECODERCONFIG_OUTOFMEMORY_RETURN",
		"GETDECODERCREATIONPARAMS_NULLPARAM",
		"GETDECODERDRIVERHANDLE_NULLPARAM",
		"GETDECODERBUFFER_NULLPARAM",
		"GETDECODERBUFFER_INVALIDBUFFER",
		"GETDECODERBUFFER_INVALIDTYPE",
		"GETDECODERBUFFER_LOCKED",
		"RELEASEDECODERBUFFER_NULLPARAM",
		"RELEASEDECODERBUFFER_INVALIDTYPE",
		"RELEASEDECODERBUFFER_NOTLOCKED",
		"DECODERBEGINFRAME_NULLPARAM",
		"DECODERBEGINFRAME_HAZARD",
		"DECODERENDFRAME_NULLPARAM",
		"SUBMITDECODERBUFFERS_NULLPARAM",
		"SUBMITDECODERBUFFERS_INVALIDTYPE",
		"DECODEREXTENSION_NULLPARAM",
		"DECODEREXTENSION_INVALIDRESOURCE",
		"CREATEVIDEOPROCESSORENUMERATOR_OUTOFMEMORY_RETURN",
		"CREATEVIDEOPROCESSORENUMERATOR_NULLPARAM",
		"CREATEVIDEOPROCESSORENUMERATOR_INVALIDFRAMEFORMAT",
		"CREATEVIDEOPROCESSORENUMERATOR_INVALIDUSAGE",
		"CREATEVIDEOPROCESSORENUMERATOR_INVALIDINPUTFRAMERATE",
		"CREATEVIDEOPROCESSORENUMERATOR_INVALIDOUTPUTFRAMERATE",
		"CREATEVIDEOPROCESSORENUMERATOR_INVALIDWIDTHHEIGHT",
		"GETVIDEOPROCESSORCONTENTDESC_NULLPARAM",
		"CHECKVIDEOPROCESSORFORMAT_NULLPARAM",
		"GETVIDEOPROCESSORCAPS_NULLPARAM",
		"GETVIDEOPROCESSORRATECONVERSIONCAPS_NULLPARAM",
		"GETVIDEOPROCESSORRATECONVERSIONCAPS_INVALIDINDEX",
		"GETVIDEOPROCESSORCUSTOMRATE_NULLPARAM",
		"GETVIDEOPROCESSORCUSTOMRATE_INVALIDINDEX",
		"GETVIDEOPROCESSORFILTERRANGE_NULLPARAM",
		"GETVIDEOPROCESSORFILTERRANGE_UNSUPPORTED",
		"CREATEVIDEOPROCESSOR_OUTOFMEMORY_RETURN",
		"CREATEVIDEOPROCESSOR_NULLPARAM",
		"VIDEOPROCESSORSETOUTPUTTARGETRECT_NULLPARAM",
		"VIDEOPROCESSORSETOUTPUTBACKGROUNDCOLOR_NULLPARAM",
		"VIDEOPROCESSORSETOUTPUTBACKGROUNDCOLOR_INVALIDALPHA",
		"VIDEOPROCESSORSETOUTPUTCOLORSPACE_NULLPARAM",
		"VIDEOPROCESSORSETOUTPUTALPHAFILLMODE_NULLPARAM",
		"VIDEOPROCESSORSETOUTPUTALPHAFILLMODE_UNSUPPORTED",
		"VIDEOPROCESSORSETOUTPUTALPHAFILLMODE_INVALIDSTREAM",
		"VIDEOPROCESSORSETOUTPUTALPHAFILLMODE_INVALIDFILLMODE",
		"VIDEOPROCESSORSETOUTPUTCONSTRICTION_NULLPARAM",
		"VIDEOPROCESSORSETOUTPUTSTEREOMODE_NULLPARAM",
		"VIDEOPROCESSORSETOUTPUTSTEREOMODE_UNSUPPORTED",
		"VIDEOPROCESSORSETOUTPUTEXTENSION_NULLPARAM",
		"VIDEOPROCESSORGETOUTPUTTARGETRECT_NULLPARAM",
		"VIDEOPROCESSORGETOUTPUTBACKGROUNDCOLOR_NULLPARAM",
		"VIDEOPROCESSORGETOUTPUTCOLORSPACE_NULLPARAM",
		"VIDEOPROCESSORGETOUTPUTALPHAFILLMODE_NULLPARAM",
		"VIDEOPROCESSORGETOUTPUTCONSTRICTION_NULLPARAM",
		"VIDEOPROCESSORSETOUTPUTCONSTRICTION_UNSUPPORTED",
		"VIDEOPROCESSORSETOUTPUTCONSTRICTION_INVALIDSIZE",
		"VIDEOPROCESSORGETOUTPUTSTEREOMODE_NULLPARAM",
		"VIDEOPROCESSORGETOUTPUTEXTENSION_NULLPARAM",
		"VIDEOPROCESSORSETSTREAMFRAMEFORMAT_NULLPARAM",
		"VIDEOPROCESSORSETSTREAMFRAMEFORMAT_INVALIDFORMAT",
		"VIDEOPROCESSORSETSTREAMFRAMEFORMAT_INVALIDSTREAM",
		"VIDEOPROCESSORSETSTREAMCOLORSPACE_NULLPARAM",
		"VIDEOPROCESSORSETSTREAMCOLORSPACE_INVALIDSTREAM",
		"VIDEOPROCESSORSETSTREAMOUTPUTRATE_NULLPARAM",
		"VIDEOPROCESSORSETSTREAMOUTPUTRATE_INVALIDRATE",
		"VIDEOPROCESSORSETSTREAMOUTPUTRATE_INVALIDFLAG",
		"VIDEOPROCESSORSETSTREAMOUTPUTRATE_INVALIDSTREAM",
		"VIDEOPROCESSORSETSTREAMSOURCERECT_NULLPARAM",
		"VIDEOPROCESSORSETSTREAMSOURCERECT_INVALIDSTREAM",
		"VIDEOPROCESSORSETSTREAMSOURCERECT_INVALIDRECT",
		"VIDEOPROCESSORSETSTREAMDESTRECT_NULLPARAM",
		"VIDEOPROCESSORSETSTREAMDESTRECT_INVALIDSTREAM",
		"VIDEOPROCESSORSETSTREAMDESTRECT_INVALIDRECT",
		"VIDEOPROCESSORSETSTREAMALPHA_NULLPARAM",
		"VIDEOPROCESSORSETSTREAMALPHA_INVALIDSTREAM",
		"VIDEOPROCESSORSETSTREAMALPHA_INVALIDALPHA",
		"VIDEOPROCESSORSETSTREAMPALETTE_NULLPARAM",
		"VIDEOPROCESSORSETSTREAMPALETTE_INVALIDSTREAM",
		"VIDEOPROCESSORSETSTREAMPALETTE_INVALIDCOUNT",
		"VIDEOPROCESSORSETSTREAMPALETTE_INVALIDALPHA",
		"VIDEOPROCESSORSETSTREAMPIXELASPECTRATIO_NULLPARAM",
		"VIDEOPROCESSORSETSTREAMPIXELASPECTRATIO_INVALIDSTREAM",
		"VIDEOPROCESSORSETSTREAMPIXELASPECTRATIO_INVALIDRATIO",
		"VIDEOPROCESSORSETSTREAMLUMAKEY_NULLPARAM",
		"VIDEOPROCESSORSETSTREAMLUMAKEY_INVALIDSTREAM",
		"VIDEOPROCESSORSETSTREAMLUMAKEY_INVALIDRANGE",
		"VIDEOPROCESSORSETSTREAMLUMAKEY_UNSUPPORTED",
		"VIDEOPROCESSORSETSTREAMSTEREOFORMAT_NULLPARAM",
		"VIDEOPROCESSORSETSTREAMSTEREOFORMAT_INVALIDSTREAM",
		"VIDEOPROCESSORSETSTREAMSTEREOFORMAT_UNSUPPORTED",
		"VIDEOPROCESSORSETSTREAMSTEREOFORMAT_FLIPUNSUPPORTED",
		"VIDEOPROCESSORSETSTREAMSTEREOFORMAT_MONOOFFSETUNSUPPORTED",
		"VIDEOPROCESSORSETSTREAMSTEREOFORMAT_FORMATUNSUPPORTED",
		"VIDEOPROCESSORSETSTREAMSTEREOFORMAT_INVALIDFORMAT",
		"VIDEOPROCESSORSETSTREAMAUTOPROCESSINGMODE_NULLPARAM",
		"VIDEOPROCESSORSETSTREAMAUTOPROCESSINGMODE_INVALIDSTREAM",
		"VIDEOPROCESSORSETSTREAMFILTER_NULLPARAM",
		"VIDEOPROCESSORSETSTREAMFILTER_INVALIDSTREAM",
		"VIDEOPROCESSORSETSTREAMFILTER_INVALIDFILTER",
		"VIDEOPROCESSORSETSTREAMFILTER_UNSUPPORTED",
		"VIDEOPROCESSORSETSTREAMFILTER_INVALIDLEVEL",
		"VIDEOPROCESSORSETSTREAMEXTENSION_NULLPARAM",
		"VIDEOPROCESSORSETSTREAMEXTENSION_INVALIDSTREAM",
		"VIDEOPROCESSORGETSTREAMFRAMEFORMAT_NULLPARAM",
		"VIDEOPROCESSORGETSTREAMCOLORSPACE_NULLPARAM",
		"VIDEOPROCESSORGETSTREAMOUTPUTRATE_NULLPARAM",
		"VIDEOPROCESSORGETSTREAMSOURCERECT_NULLPARAM",
		"VIDEOPROCESSORGETSTREAMDESTRECT_NULLPARAM",
		"VIDEOPROCESSORGETSTREAMALPHA_NULLPARAM",
		"VIDEOPROCESSORGETSTREAMPALETTE_NULLPARAM",
		"VIDEOPROCESSORGETSTREAMPIXELASPECTRATIO_NULLPARAM",
		"VIDEOPROCESSORGETSTREAMLUMAKEY_NULLPARAM",
		"VIDEOPROCESSORGETSTREAMSTEREOFORMAT_NULLPARAM",
		"VIDEOPROCESSORGETSTREAMAUTOPROCESSINGMODE_NULLPARAM",
		"VIDEOPROCESSORGETSTREAMFILTER_NULLPARAM",
		"VIDEOPROCESSORGETSTREAMEXTENSION_NULLPARAM",
		"VIDEOPROCESSORGETSTREAMEXTENSION_INVALIDSTREAM",
		"VIDEOPROCESSORBLT_NULLPARAM",
		"VIDEOPROCESSORBLT_INVALIDSTREAMCOUNT",
		"VIDEOPROCESSORBLT_TARGETRECT",
		"VIDEOPROCESSORBLT_INVALIDOUTPUT",
		"VIDEOPROCESSORBLT_INVALIDPASTFRAMES",
		"VIDEOPROCESSORBLT_INVALIDFUTUREFRAMES",
		"VIDEOPROCESSORBLT_INVALIDSOURCERECT",
		"VIDEOPROCESSORBLT_INVALIDDESTRECT",
		"VIDEOPROCESSORBLT_INVALIDINPUTRESOURCE",
		"VIDEOPROCESSORBLT_INVALIDARRAYSIZE",
		"VIDEOPROCESSORBLT_INVALIDARRAY",
		"VIDEOPROCESSORBLT_RIGHTEXPECTED",
		"VIDEOPROCESSORBLT_RIGHTNOTEXPECTED",
		"VIDEOPROCESSORBLT_STEREONOTENABLED",
		"VIDEOPROCESSORBLT_INVALIDRIGHTRESOURCE",
		"VIDEOPROCESSORBLT_NOSTEREOSTREAMS",
		"VIDEOPROCESSORBLT_INPUTHAZARD",
		"VIDEOPROCESSORBLT_OUTPUTHAZARD",
		"CREATEVIDEODECODEROUTPUTVIEW_OUTOFMEMORY_RETURN",
		"CREATEVIDEODECODEROUTPUTVIEW_NULLPARAM",
		"CREATEVIDEODECODEROUTPUTVIEW_INVALIDTYPE",
		"CREATEVIDEODECODEROUTPUTVIEW_INVALIDBIND",
		"CREATEVIDEODECODEROUTPUTVIEW_UNSUPPORTEDFORMAT",
		"CREATEVIDEODECODEROUTPUTVIEW_INVALIDMIP",
		"CREATEVIDEODECODEROUTPUTVIEW_UNSUPPORTEMIP",
		"CREATEVIDEODECODEROUTPUTVIEW_INVALIDARRAYSIZE",
		"CREATEVIDEODECODEROUTPUTVIEW_INVALIDARRAY",
		"CREATEVIDEODECODEROUTPUTVIEW_INVALIDDIMENSION",
		"CREATEVIDEOPROCESSORINPUTVIEW_OUTOFMEMORY_RETURN",
		"CREATEVIDEOPROCESSORINPUTVIEW_NULLPARAM",
		"CREATEVIDEOPROCESSORINPUTVIEW_INVALIDTYPE",
		"CREATEVIDEOPROCESSORINPUTVIEW_INVALIDBIND",
		"CREATEVIDEOPROCESSORINPUTVIEW_INVALIDMISC",
		"CREATEVIDEOPROCESSORINPUTVIEW_INVALIDUSAGE",
		"CREATEVIDEOPROCESSORINPUTVIEW_INVALIDFORMAT",
		"CREATEVIDEOPROCESSORINPUTVIEW_INVALIDFOURCC",
		"CREATEVIDEOPROCESSORINPUTVIEW_INVALIDMIP",
		"CREATEVIDEOPROCESSORINPUTVIEW_UNSUPPORTEDMIP",
		"CREATEVIDEOPROCESSORINPUTVIEW_INVALIDARRAYSIZE",
		"CREATEVIDEOPROCESSORINPUTVIEW_INVALIDARRAY",
		"CREATEVIDEOPROCESSORINPUTVIEW_INVALIDDIMENSION",
		"CREATEVIDEOPROCESSOROUTPUTVIEW_OUTOFMEMORY_RETURN",
		"CREATEVIDEOPROCESSOROUTPUTVIEW_NULLPARAM",
		"CREATEVIDEOPROCESSOROUTPUTVIEW_INVALIDTYPE",
		"CREATEVIDEOPROCESSOROUTPUTVIEW_INVALIDBIND",
		"CREATEVIDEOPROCESSOROUTPUTVIEW_INVALIDFORMAT",
		"CREATEVIDEOPROCESSOROUTPUTVIEW_INVALIDMIP",
		"CREATEVIDEOPROCESSOROUTPUTVIEW_UNSUPPORTEDMIP",
		"CREATEVIDEOPROCESSOROUTPUTVIEW_UNSUPPORTEDARRAY",
		"CREATEVIDEOPROCESSOROUTPUTVIEW_INVALIDARRAY",
		"CREATEVIDEOPROCESSOROUTPUTVIEW_INVALIDDIMENSION",
		"DEVICE_DRAW_INVALID_USE_OF_FORCED_SAMPLE_COUNT",
		"CREATEBLENDSTATE_INVALIDLOGICOPS",
		"CREATESHADERRESOURCEVIEW_INVALIDDARRAYWITHDECODER",
		"CREATEUNORDEREDACCESSVIEW_INVALIDDARRAYWITHDECODER",
		"CREATERENDERTARGETVIEW_INVALIDDARRAYWITHDECODER",
		"DEVICE_LOCKEDOUT_INTERFACE",
		"REF_WARNING_ATOMIC_INCONSISTENT",
		"REF_WARNING_READING_UNINITIALIZED_RESOURCE",
		"REF_WARNING_RAW_HAZARD",
		"REF_WARNING_WAR_HAZARD",
		"REF_WARNING_WAW_HAZARD",
		"CREATECRYPTOSESSION_NULLPARAM",
		"CREATECRYPTOSESSION_OUTOFMEMORY_RETURN",
		"GETCRYPTOTYPE_NULLPARAM",
		"GETDECODERPROFILE_NULLPARAM",
		"GETCRYPTOSESSIONCERTIFICATESIZE_NULLPARAM",
		"GETCRYPTOSESSIONCERTIFICATE_NULLPARAM",
		"GETCRYPTOSESSIONCERTIFICATE_WRONGSIZE",
		"GETCRYPTOSESSIONHANDLE_WRONGSIZE",
		"NEGOTIATECRPYTOSESSIONKEYEXCHANGE_NULLPARAM",
		"ENCRYPTIONBLT_UNSUPPORTED",
		"ENCRYPTIONBLT_NULLPARAM",
		"ENCRYPTIONBLT_SRC_WRONGDEVICE",
		"ENCRYPTIONBLT_DST_WRONGDEVICE",
		"ENCRYPTIONBLT_FORMAT_MISMATCH",
		"ENCRYPTIONBLT_SIZE_MISMATCH",
		"ENCRYPTIONBLT_SRC_MULTISAMPLED",
		"ENCRYPTIONBLT_DST_NOT_STAGING",
		"ENCRYPTIONBLT_SRC_MAPPED",
		"ENCRYPTIONBLT_DST_MAPPED",
		"ENCRYPTIONBLT_SRC_OFFERED",
		"ENCRYPTIONBLT_DST_OFFERED",
		"ENCRYPTIONBLT_SRC_CONTENT_UNDEFINED",
		"DECRYPTIONBLT_UNSUPPORTED",
		"DECRYPTIONBLT_NULLPARAM",
		"DECRYPTIONBLT_SRC_WRONGDEVICE",
		"DECRYPTIONBLT_DST_WRONGDEVICE",
		"DECRYPTIONBLT_FORMAT_MISMATCH",
		"DECRYPTIONBLT_SIZE_MISMATCH",
		"DECRYPTIONBLT_DST_MULTISAMPLED",
		"DECRYPTIONBLT_SRC_NOT_STAGING",
		"DECRYPTIONBLT_DST_NOT_RENDER_TARGET",
		"DECRYPTIONBLT_SRC_MAPPED",
		"DECRYPTIONBLT_DST_MAPPED",
		"DECRYPTIONBLT_SRC_OFFERED",
		"DECRYPTIONBLT_DST_OFFERED",
		"DECRYPTIONBLT_SRC_CONTENT_UNDEFINED",
		"STARTSESSIONKEYREFRESH_NULLPARAM",
		"STARTSESSIONKEYREFRESH_INVALIDSIZE",
		"FINISHSESSIONKEYREFRESH_NULLPARAM",
		"GETENCRYPTIONBLTKEY_NULLPARAM",
		"GETENCRYPTIONBLTKEY_INVALIDSIZE",
		"GETCONTENTPROTECTIONCAPS_NULLPARAM",
		"CHECKCRYPTOKEYEXCHANGE_NULLPARAM",
		"CHECKCRYPTOKEYEXCHANGE_INVALIDINDEX",
		"CREATEAUTHENTICATEDCHANNEL_NULLPARAM",
		"CREATEAUTHENTICATEDCHANNEL_UNSUPPORTED",
		"CREATEAUTHENTICATEDCHANNEL_INVALIDTYPE",
		"CREATEAUTHENTICATEDCHANNEL_OUTOFMEMORY_RETURN",
		"GETAUTHENTICATEDCHANNELCERTIFICATESIZE_INVALIDCHANNEL",
		"GETAUTHENTICATEDCHANNELCERTIFICATESIZE_NULLPARAM",
		"GETAUTHENTICATEDCHANNELCERTIFICATE_INVALIDCHANNEL",
		"GETAUTHENTICATEDCHANNELCERTIFICATE_NULLPARAM",
		"GETAUTHENTICATEDCHANNELCERTIFICATE_WRONGSIZE",
		"NEGOTIATEAUTHENTICATEDCHANNELKEYEXCHANGE_INVALIDCHANNEL",
		"NEGOTIATEAUTHENTICATEDCHANNELKEYEXCHANGE_NULLPARAM",
		"QUERYAUTHENTICATEDCHANNEL_NULLPARAM",
		"QUERYAUTHENTICATEDCHANNEL_WRONGCHANNEL",
		"QUERYAUTHENTICATEDCHANNEL_UNSUPPORTEDQUERY",
		"QUERYAUTHENTICATEDCHANNEL_WRONGSIZE",
		"QUERYAUTHENTICATEDCHANNEL_INVALIDPROCESSINDEX",
		"CONFIGUREAUTHENTICATEDCHANNEL_NULLPARAM",
		"CONFIGUREAUTHENTICATEDCHANNEL_WRONGCHANNEL",
		"CONFIGUREAUTHENTICATEDCHANNEL_UNSUPPORTEDCONFIGURE",
		"CONFIGUREAUTHENTICATEDCHANNEL_WRONGSIZE",
		"CONFIGUREAUTHENTICATEDCHANNEL_INVALIDPROCESSIDTYPE",
		"VSSETCONSTANTBUFFERS_INVALIDBUFFEROFFSETORCOUNT",
		"DSSETCONSTANTBUFFERS_INVALIDBUFFEROFFSETORCOUNT",
		"HSSETCONSTANTBUFFERS_INVALIDBUFFEROFFSETORCOUNT",
		"GSSETCONSTANTBUFFERS_INVALIDBUFFEROFFSETORCOUNT",
		"PSSETCONSTANTBUFFERS_INVALIDBUFFEROFFSETORCOUNT",
		"CSSETCONSTANTBUFFERS_INVALIDBUFFEROFFSETORCOUNT",
		"NEGOTIATECRPYTOSESSIONKEYEXCHANGE_INVALIDSIZE",
		"NEGOTIATEAUTHENTICATEDCHANNELKEYEXCHANGE_INVALIDSIZE",
		"OFFERRESOURCES_INVALIDPRIORITY",
		"GETCRYPTOSESSIONHANDLE_OUTOFMEMORY",
		"ACQUIREHANDLEFORCAPTURE_NULLPARAM",
		"ACQUIREHANDLEFORCAPTURE_INVALIDTYPE",
		"ACQUIREHANDLEFORCAPTURE_INVALIDBIND",
		"ACQUIREHANDLEFORCAPTURE_INVALIDARRAY",
		"VIDEOPROCESSORSETSTREAMROTATION_NULLPARAM",
		"VIDEOPROCESSORSETSTREAMROTATION_INVALIDSTREAM",
		"VIDEOPROCESSORSETSTREAMROTATION_INVALID",
		"VIDEOPROCESSORSETSTREAMROTATION_UNSUPPORTED",
		"VIDEOPROCESSORGETSTREAMROTATION_NULLPARAM",
		"DEVICE_CLEARVIEW_INVALIDVIEW",
		"DEVICE_CREATEVERTEXSHADER_DOUBLEEXTENSIONSNOTSUPPORTED",
		"DEVICE_CREATEVERTEXSHADER_SHADEREXTENSIONSNOTSUPPORTED",
		"DEVICE_CREATEHULLSHADER_DOUBLEEXTENSIONSNOTSUPPORTED",
		"DEVICE_CREATEHULLSHADER_SHADEREXTENSIONSNOTSUPPORTED",
		"DEVICE_CREATEDOMAINSHADER_DOUBLEEXTENSIONSNOTSUPPORTED",
		"DEVICE_CREATEDOMAINSHADER_SHADEREXTENSIONSNOTSUPPORTED",
		"DEVICE_CREATEGEOMETRYSHADER_DOUBLEEXTENSIONSNOTSUPPORTED",
		"DEVICE_CREATEGEOMETRYSHADER_SHADEREXTENSIONSNOTSUPPORTED",
		"DEVICE_CREATEGEOMETRYSHADERWITHSTREAMOUTPUT_DOUBLEEXTENSIONSNOTSUPPORTED",
		"DEVICE_CREATEGEOMETRYSHADERWITHSTREAMOUTPUT_SHADEREXTENSIONSNOTSUPPORTED",
		"DEVICE_CREATEPIXELSHADER_DOUBLEEXTENSIONSNOTSUPPORTED",
		"DEVICE_CREATEPIXELSHADER_SHADEREXTENSIONSNOTSUPPORTED",
		"DEVICE_CREATECOMPUTESHADER_DOUBLEEXTENSIONSNOTSUPPORTED",
		"DEVICE_CREATECOMPUTESHADER_SHADEREXTENSIONSNOTSUPPORTED",
		"DEVICE_SHADER_LINKAGE_MINPRECISION",
		"VIDEOPROCESSORSETSTREAMALPHA_UNSUPPORTED",
		"VIDEOPROCESSORSETSTREAMPIXELASPECTRATIO_UNSUPPORTED",
		"DEVICE_CREATEVERTEXSHADER_UAVSNOTSUPPORTED",
		"DEVICE_CREATEHULLSHADER_UAVSNOTSUPPORTED",
		"DEVICE_CREATEDOMAINSHADER_UAVSNOTSUPPORTED",
		"DEVICE_CREATEGEOMETRYSHADER_UAVSNOTSUPPORTED",
		"DEVICE_CREATEGEOMETRYSHADERWITHSTREAMOUTPUT_UAVSNOTSUPPORTED",
		"DEVICE_CREATEPIXELSHADER_UAVSNOTSUPPORTED",
		"DEVICE_CREATECOMPUTESHADER_UAVSNOTSUPPORTED",
		"DEVICE_OMSETRENDERTARGETSANDUNORDEREDACCESSVIEWS_INVALIDOFFSET",
		"DEVICE_OMSETRENDERTARGETSANDUNORDEREDACCESSVIEWS_TOOMANYVIEWS",
		"DEVICE_CLEARVIEW_NOTSUPPORTED",
		"SWAPDEVICECONTEXTSTATE_NOTSUPPORTED",
		"UPDATESUBRESOURCE_PREFERUPDATESUBRESOURCE1",
		"GETDC_INACCESSIBLE",
		"DEVICE_CLEARVIEW_INVALIDRECT",
		"DEVICE_DRAW_SAMPLE_MASK_IGNORED_ON_FL9",
		"DEVICE_OPEN_SHARED_RESOURCE1_NOT_SUPPORTED",
		"DEVICE_OPEN_SHARED_RESOURCE_BY_NAME_NOT_SUPPORTED",
		"ENQUEUESETEVENT_NOT_SUPPORTED",
		"OFFERRELEASE_NOT_SUPPORTED",
		"OFFERRESOURCES_INACCESSIBLE",
		"CREATEVIDEOPROCESSORINPUTVIEW_INVALIDMSAA",
		"CREATEVIDEOPROCESSOROUTPUTVIEW_INVALIDMSAA",
		"DEVICE_CLEARVIEW_INVALIDSOURCERECT",
		"DEVICE_CLEARVIEW_EMPTYRECT",
		"UPDATESUBRESOURCE_EMPTYDESTBOX",
		"COPYSUBRESOURCEREGION_EMPTYSOURCEBOX",
		"DEVICE_DRAW_OM_RENDER_TARGET_DOES_NOT_SUPPORT_LOGIC_OPS",
		"DEVICE_DRAW_DEPTHSTENCILVIEW_NOT_SET",
		"DEVICE_DRAW_RENDERTARGETVIEW_NOT_SET",
		"DEVICE_DRAW_RENDERTARGETVIEW_NOT_SET_DUE_TO_FLIP_PRESENT",
		"DEVICE_UNORDEREDACCESSVIEW_NOT_SET_DUE_TO_FLIP_PRESENT",
		"GETDATAFORNEWHARDWAREKEY_NULLPARAM",
		"CHECKCRYPTOSESSIONSTATUS_NULLPARAM",
		"GETCRYPTOSESSIONPRIVATEDATASIZE_NULLPARAM",
		"GETVIDEODECODERCAPS_NULLPARAM",
		"GETVIDEODECODERCAPS_ZEROWIDTHHEIGHT",
		"CHECKVIDEODECODERDOWNSAMPLING_NULLPARAM",
		"CHECKVIDEODECODERDOWNSAMPLING_INVALIDCOLORSPACE",
		"CHECKVIDEODECODERDOWNSAMPLING_ZEROWIDTHHEIGHT",
		"VIDEODECODERENABLEDOWNSAMPLING_NULLPARAM",
		"VIDEODECODERENABLEDOWNSAMPLING_UNSUPPORTED",
		"VIDEODECODERUPDATEDOWNSAMPLING_NULLPARAM",
		"VIDEODECODERUPDATEDOWNSAMPLING_UNSUPPORTED",
		"CHECKVIDEOPROCESSORFORMATCONVERSION_NULLPARAM",
		"VIDEOPROCESSORSETOUTPUTCOLORSPACE1_NULLPARAM",
		"VIDEOPROCESSORGETOUTPUTCOLORSPACE1_NULLPARAM",
		"VIDEOPROCESSORSETSTREAMCOLORSPACE1_NULLPARAM",
		"VIDEOPROCESSORSETSTREAMCOLORSPACE1_INVALIDSTREAM",
		"VIDEOPROCESSORSETSTREAMMIRROR_NULLPARAM",
		"VIDEOPROCESSORSETSTREAMMIRROR_INVALIDSTREAM",
		"VIDEOPROCESSORSETSTREAMMIRROR_UNSUPPORTED",
		"VIDEOPROCESSORGETSTREAMCOLORSPACE1_NULLPARAM",
		"VIDEOPROCESSORGETSTREAMMIRROR_NULLPARAM",
		"RECOMMENDVIDEODECODERDOWNSAMPLING_NULLPARAM",
		"RECOMMENDVIDEODECODERDOWNSAMPLING_INVALIDCOLORSPACE",
		"RECOMMENDVIDEODECODERDOWNSAMPLING_ZEROWIDTHHEIGHT",
		"VIDEOPROCESSORSETOUTPUTSHADERUSAGE_NULLPARAM",
		"VIDEOPROCESSORGETOUTPUTSHADERUSAGE_NULLPARAM",
		"VIDEOPROCESSORGETBEHAVIORHINTS_NULLPARAM",
		"VIDEOPROCESSORGETBEHAVIORHINTS_INVALIDSTREAMCOUNT",
		"VIDEOPROCESSORGETBEHAVIORHINTS_TARGETRECT",
		"VIDEOPROCESSORGETBEHAVIORHINTS_INVALIDSOURCERECT",
		"VIDEOPROCESSORGETBEHAVIORHINTS_INVALIDDESTRECT",
		"GETCRYPTOSESSIONPRIVATEDATASIZE_INVALID_KEY_EXCHANGE_TYPE",
		"DEVICE_OPEN_SHARED_RESOURCE1_ACCESS_DENIED",
		"D3D11_1_MESSAGES_END",
		"D3D11_2_MESSAGES_START",
		"CREATEBUFFER_INVALIDUSAGE",
		"CREATETEXTURE1D_INVALIDUSAGE",
		"CREATETEXTURE2D_INVALIDUSAGE",
		"CREATEINPUTLAYOUT_LEVEL9_STEPRATE_NOT_1",
		"CREATEINPUTLAYOUT_LEVEL9_INSTANCING_NOT_SUPPORTED",
		"UPDATETILEMAPPINGS_INVALID_PARAMETER",
		"COPYTILEMAPPINGS_INVALID_PARAMETER",
		"COPYTILES_INVALID_PARAMETER",
		"UPDATETILES_INVALID_PARAMETER",
		"RESIZETILEPOOL_INVALID_PARAMETER",
		"TILEDRESOURCEBARRIER_INVALID_PARAMETER",
		"NULL_TILE_MAPPING_ACCESS_WARNING",
		"NULL_TILE_MAPPING_ACCESS_ERROR",
		"DIRTY_TILE_MAPPING_ACCESS",
		"DUPLICATE_TILE_MAPPINGS_IN_COVERED_AREA",
		"TILE_MAPPINGS_IN_COVERED_AREA_DUPLICATED_OUTSIDE",
		"TILE_MAPPINGS_SHARED_BETWEEN_INCOMPATIBLE_RESOURCES",
		"TILE_MAPPINGS_SHARED_BETWEEN_INPUT_AND_OUTPUT",
		"CHECKMULTISAMPLEQUALITYLEVELS_INVALIDFLAGS",
		"GETRESOURCETILING_NONTILED_RESOURCE",
		"RESIZETILEPOOL_SHRINK_WITH_MAPPINGS_STILL_DEFINED_PAST_END",
		"NEED_TO_CALL_TILEDRESOURCEBARRIER",
		"CREATEDEVICE_INVALIDARGS",
		"CREATEDEVICE_WARNING",
		"CLEARUNORDEREDACCESSVIEWUINT_HAZARD",
		"CLEARUNORDEREDACCESSVIEWFLOAT_HAZARD",
		"TILED_RESOURCE_TIER_1_BUFFER_TEXTURE_MISMATCH",
		"CREATE_CRYPTOSESSION",
		"CREATE_AUTHENTICATEDCHANNEL",
		"LIVE_CRYPTOSESSION",
		"LIVE_AUTHENTICATEDCHANNEL",
		"DESTROY_CRYPTOSESSION",
		"DESTROY_AUTHENTICATEDCHANNEL",
		"D3D11_2_MESSAGES_END",
		"D3D11_3_MESSAGES_START",
		"CREATERASTERIZERSTATE_INVALID_CONSERVATIVERASTERMODE",
		"DEVICE_DRAW_INVALID_SYSTEMVALUE",
		"CREATEQUERYORPREDICATE_INVALIDCONTEXTTYPE",
		"CREATEQUERYORPREDICATE_DECODENOTSUPPORTED",
		"CREATEQUERYORPREDICATE_ENCODENOTSUPPORTED",
		"CREATESHADERRESOURCEVIEW_INVALIDPLANEINDEX",
		"CREATESHADERRESOURCEVIEW_INVALIDVIDEOPLANEINDEX",
		"CREATESHADERRESOURCEVIEW_AMBIGUOUSVIDEOPLANEINDEX",
		"CREATERENDERTARGETVIEW_INVALIDPLANEINDEX",
		"CREATERENDERTARGETVIEW_INVALIDVIDEOPLANEINDEX",
		"CREATERENDERTARGETVIEW_AMBIGUOUSVIDEOPLANEINDEX",
		"CREATEUNORDEREDACCESSVIEW_INVALIDPLANEINDEX",
		"CREATEUNORDEREDACCESSVIEW_INVALIDVIDEOPLANEINDEX",
		"CREATEUNORDEREDACCESSVIEW_AMBIGUOUSVIDEOPLANEINDEX",
		"JPEGDECODE_INVALIDSCANDATAOFFSET",
		"JPEGDECODE_NOTSUPPORTED",
		"JPEGDECODE_DIMENSIONSTOOLARGE",
		"JPEGDECODE_INVALIDCOMPONENTS",
		"JPEGDECODE_DESTINATIONNOT2D",
		"JPEGDECODE_TILEDRESOURCESUNSUPPORTED",
		"JPEGDECODE_GUARDRECTSUNSUPPORTED",
		"JPEGDECODE_FORMATUNSUPPORTED",
		"JPEGDECODE_INVALIDSUBRESOURCE",
		"JPEGDECODE_INVALIDMIPLEVEL",
		"JPEGDECODE_EMPTYDESTBOX",
		"JPEGDECODE_DESTBOXNOT2D",
		"JPEGDECODE_DESTBOXNOTSUB",
		"JPEGDECODE_DESTBOXESINTERSECT",
		"JPEGDECODE_XSUBSAMPLEMISMATCH",
		"JPEGDECODE_YSUBSAMPLEMISMATCH",
		"JPEGDECODE_XSUBSAMPLEODD",
		"JPEGDECODE_YSUBSAMPLEODD",
		"JPEGDECODE_OUTPUTDIMENSIONSTOOLARGE",
		"JPEGDECODE_NONPOW2SCALEUNSUPPORTED",
		"JPEGDECODE_FRACTIONALDOWNSCALETOLARGE",
		"JPEGDECODE_CHROMASIZEMISMATCH",
		"JPEGDECODE_LUMACHROMASIZEMISMATCH",
		"JPEGDECODE_INVALIDNUMDESTINATIONS",
		"JPEGDECODE_SUBBOXUNSUPPORTED",
		"JPEGDECODE_1DESTUNSUPPORTEDFORMAT",
		"JPEGDECODE_3DESTUNSUPPORTEDFORMAT",
		"JPEGDECODE_SCALEUNSUPPORTED",
		"JPEGDECODE_INVALIDSOURCESIZE",
		"JPEGDECODE_INVALIDCOPYFLAGS",
		"JPEGDECODE_HAZARD",
		"JPEGDECODE_UNSUPPORTEDSRCBUFFERUSAGE",
		"JPEGDECODE_UNSUPPORTEDSRCBUFFERMISCFLAGS",
		"JPEGDECODE_UNSUPPORTEDDSTTEXTUREUSAGE",
		"JPEGDECODE_BACKBUFFERNOTSUPPORTED",
		"JPEGDECODE_UNSUPPRTEDCOPYFLAGS",
		"JPEGENCODE_NOTSUPPORTED",
		"JPEGENCODE_INVALIDSCANDATAOFFSET",
		"JPEGENCODE_INVALIDCOMPONENTS",
		"JPEGENCODE_SOURCENOT2D",
		"JPEGENCODE_TILEDRESOURCESUNSUPPORTED",
		"JPEGENCODE_GUARDRECTSUNSUPPORTED",
		"JPEGENCODE_XSUBSAMPLEMISMATCH",
		"JPEGENCODE_YSUBSAMPLEMISMATCH",
		"JPEGENCODE_FORMATUNSUPPORTED",
		"JPEGENCODE_INVALIDSUBRESOURCE",
		"JPEGENCODE_INVALIDMIPLEVEL",
		"JPEGENCODE_DIMENSIONSTOOLARGE",
		"JPEGENCODE_HAZARD",
		"JPEGENCODE_UNSUPPORTEDDSTBUFFERUSAGE",
		"JPEGENCODE_UNSUPPORTEDDSTBUFFERMISCFLAGS",
		"JPEGENCODE_UNSUPPORTEDSRCTEXTUREUSAGE",
		"JPEGENCODE_BACKBUFFERNOTSUPPORTED",
		"CREATEQUERYORPREDICATE_UNSUPPORTEDCONTEXTTTYPEFORQUERY",
		"FLUSH1_INVALIDCONTEXTTYPE",
		"DEVICE_SETHARDWAREPROTECTION_INVALIDCONTEXT",
		"VIDEOPROCESSORSETOUTPUTHDRMETADATA_NULLPARAM",
		"VIDEOPROCESSORSETOUTPUTHDRMETADATA_INVALIDSIZE",
		"VIDEOPROCESSORGETOUTPUTHDRMETADATA_NULLPARAM",
		"VIDEOPROCESSORGETOUTPUTHDRMETADATA_INVALIDSIZE",
		"VIDEOPROCESSORSETSTREAMHDRMETADATA_NULLPARAM",
		"VIDEOPROCESSORSETSTREAMHDRMETADATA_INVALIDSTREAM",
		"VIDEOPROCESSORSETSTREAMHDRMETADATA_INVALIDSIZE",
		"VIDEOPROCESSORGETSTREAMHDRMETADATA_NULLPARAM",
		"VIDEOPROCESSORGETSTREAMHDRMETADATA_INVALIDSTREAM",
		"VIDEOPROCESSORGETSTREAMHDRMETADATA_INVALIDSIZE",
		"VIDEOPROCESSORGETSTREAMFRAMEFORMAT_INVALIDSTREAM",
		"VIDEOPROCESSORGETSTREAMCOLORSPACE_INVALIDSTREAM",
		"VIDEOPROCESSORGETSTREAMOUTPUTRATE_INVALIDSTREAM",
		"VIDEOPROCESSORGETSTREAMSOURCERECT_INVALIDSTREAM",
		"VIDEOPROCESSORGETSTREAMDESTRECT_INVALIDSTREAM",
		"VIDEOPROCESSORGETSTREAMALPHA_INVALIDSTREAM",
		"VIDEOPROCESSORGETSTREAMPALETTE_INVALIDSTREAM",
		"VIDEOPROCESSORGETSTREAMPIXELASPECTRATIO_INVALIDSTREAM",
		"VIDEOPROCESSORGETSTREAMLUMAKEY_INVALIDSTREAM",
		"VIDEOPROCESSORGETSTREAMSTEREOFORMAT_INVALIDSTREAM",
		"VIDEOPROCESSORGETSTREAMAUTOPROCESSINGMODE_INVALIDSTREAM",
		"VIDEOPROCESSORGETSTREAMFILTER_INVALIDSTREAM",
		"VIDEOPROCESSORGETSTREAMROTATION_INVALIDSTREAM",
		"VIDEOPROCESSORGETSTREAMCOLORSPACE1_INVALIDSTREAM",
		"VIDEOPROCESSORGETSTREAMMIRROR_INVALIDSTREAM",
		"CREATE_FENCE",
		"LIVE_FENCE",
		"DESTROY_FENCE",
		"CREATE_SYNCHRONIZEDCHANNEL",
		"LIVE_SYNCHRONIZEDCHANNEL",
		"DESTROY_SYNCHRONIZEDCHANNEL",
		"CREATEFENCE_INVALIDFLAGS",
		"D3D11_3_MESSAGES_END",
		"D3D11_5_MESSAGES_START",
		"NEGOTIATECRYPTOSESSIONKEYEXCHANGEMT_INVALIDKEYEXCHANGETYPE",
		"NEGOTIATECRYPTOSESSIONKEYEXCHANGEMT_NOT_SUPPORTED",
		"DECODERBEGINFRAME_INVALID_HISTOGRAM_COMPONENT_COUNT",
		"DECODERBEGINFRAME_INVALID_HISTOGRAM_COMPONENT",
		"DECODERBEGINFRAME_INVALID_HISTOGRAM_BUFFER_SIZE",
		"DECODERBEGINFRAME_INVALID_HISTOGRAM_BUFFER_USAGE",
		"DECODERBEGINFRAME_INVALID_HISTOGRAM_BUFFER_MISC_FLAGS",
		"DECODERBEGINFRAME_INVALID_HISTOGRAM_BUFFER_OFFSET",
		"CREATE_TRACKEDWORKLOAD",
		"LIVE_TRACKEDWORKLOAD",
		"DESTROY_TRACKEDWORKLOAD",
		"CREATE_TRACKED_WORKLOAD_NULLPARAM",
		"CREATE_TRACKED_WORKLOAD_INVALID_MAX_INSTANCES",
		"CREATE_TRACKED_WORKLOAD_INVALID_DEADLINE_TYPE",
		"CREATE_TRACKED_WORKLOAD_INVALID_ENGINE_TYPE",
		"MULTIPLE_TRACKED_WORKLOADS",
		"MULTIPLE_TRACKED_WORKLOAD_PAIRS",
		"INCOMPLETE_TRACKED_WORKLOAD_PAIR",
		"OUT_OF_ORDER_TRACKED_WORKLOAD_PAIR",
		"CANNOT_ADD_TRACKED_WORKLOAD",
		"TRACKED_WORKLOAD_NOT_SUPPORTED",
		"TRACKED_WORKLOAD_ENGINE_TYPE_NOT_FOUND",
		"NO_TRACKED_WORKLOAD_SLOT_AVAILABLE",
		"END_TRACKED_WORKLOAD_INVALID_ARG",
		"TRACKED_WORKLOAD_DISJOINT_FAILURE",
		"D3D11_5_MESSAGES_END"
	};

	UINT64 numMessages = m_debugInfoQueue->GetNumStoredMessages();

	for (int i = 0; i < numMessages; ++i) {
		SIZE_T messageSize = 0;
		m_debugInfoQueue->GetMessageA(i, nullptr, &messageSize);

		// @TODO: use scratch memory / allocators
		D3D11_MESSAGE* message = (D3D11_MESSAGE*)malloc(messageSize);
		m_debugInfoQueue->GetMessageA(i, message, &messageSize);

		using level_enum = spdlog::level::level_enum;
		level_enum loglevel = level_enum::critical;

		switch(message->Severity) {
		case D3D11_MESSAGE_SEVERITY_CORRUPTION:
			loglevel = level_enum::critical;
			break;
		case D3D11_MESSAGE_SEVERITY_ERROR:
			loglevel = level_enum::err;
			break;
		case D3D11_MESSAGE_SEVERITY_WARNING:
			loglevel = level_enum::warn;
			break;
		case D3D11_MESSAGE_SEVERITY_INFO:
			loglevel = level_enum::info;
			break;
		case D3D11_MESSAGE_SEVERITY_MESSAGE:
			loglevel = level_enum::trace;
			break; 
		}

		spdlog::log(loglevel, "{} {} {}", messageIdStrings[message->ID], messageCategoryStrings[message->Category], message->pDescription);

		free(message);
	}

	m_debugInfoQueue->ClearStoredMessages();
}

void Renderer::VerifyGraphicsPipeline() {
	if(auto res = m_debug->ValidateContext(m_deviceContext.Get()); FAILED(res)) {
		DXERROR(res);
	}
}

void Renderer::VerifyComputePipeline() {
	if(auto res = m_debug->ValidateContextForDispatch(m_deviceContext.Get()); FAILED(res)) {
		DXERROR(res);
	}
}
