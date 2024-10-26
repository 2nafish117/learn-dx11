#include "DX11Context.hpp"

#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include <imgui.h>
#include <backends/imgui_impl_dx11.h>
#include <backends/imgui_impl_glfw.h>

#include <comdef.h>

#include "Mesh.hpp"
#include "Shader.hpp"
#include "Camera.hpp"

DX11Context::DX11Context(GLFWwindow* window)
	: m_window(window)
{
	m_scratchSize = 4096;
	m_scratchMemory = new byte[m_scratchSize];

	ASSERT(m_window != nullptr, "");

	UINT factoryCreateFlags = 0;

// @TODO: make a DXDEBUG flag
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

	// std::vector<StaticMesh::Vertex> vertices = {
	// 	StaticMesh::Vertex{ float3(-0.5f, -0.5f, 0.5f), float3(0.0f, 0.0f, -1.0f), float3(0.0f, 0.5f, 0.0f), float2(0.0f, 1.0f) },
	// 	StaticMesh::Vertex{ float3(0.5f, 0.5f, 0.5f),   float3(0.0f, 0.0f, -1.0f), float3(0.0f, 0.0f, 0.5f), float2(1.0f, 0.0f) },
	// 	StaticMesh::Vertex{ float3(0.5f, -0.5f, 0.5f),  float3(0.0f, 0.0f, -1.0f), float3(0.0f, 0.0f, 0.5f), float2(1.0f, 1.0f) },
	// 	StaticMesh::Vertex{ float3(-0.5f, 0.5f, 0.5f),  float3(0.0f, 0.0f, -1.0f), float3(0.5f, 0.0f, 0.0f), float2(0.0f, 0.0f) },
	// };

	std::vector<float3> positions = {
		float3(-0.5f, -0.5f, 0.5f), float3(0.5f, 0.5f, 0.5f), float3(0.5f, -0.5f, 0.5f), float3(-0.5f, 0.5f, 0.5f)
	};
	std::vector<float3> normals = {
		float3(0.0f, 0.0f, -1.0f),
		float3(0.0f, 0.0f, -1.0f),
		float3(0.0f, 0.0f, -1.0f),
		float3(0.0f, 0.0f, -1.0f),
	};
	std::vector<float3> tangents;
	std::vector<float3> colors = {
		float3(0.0f, 0.5f, 0.0f),
		float3(0.0f, 0.0f, 0.5f),
		float3(0.0f, 0.0f, 0.5f),
		float3(0.5f, 0.0f, 0.0f),
	};
	std::vector<float2> uv0s = {
		float2(0.0f, 1.0f),
		float2(1.0f, 0.0f),
		float2(1.0f, 1.0f),
		float2(0.0f, 0.0f),
	};
	std::vector<float2> uv1s;

	std::vector<u32> indices = {
		0, 1, 2,
		0, 3, 1
	};

	m_quadMeshAsset = std::make_shared<MeshAsset>(positions, normals, tangents, colors, uv0s, uv1s, indices);
	m_quadMesh = std::make_shared<StaticMesh>(m_device, m_quadMeshAsset);

	m_cubeMeshAsset = std::make_shared<MeshAsset>("data/meshes/Box.glb");
	m_cubeMesh = std::make_shared<StaticMesh>(m_device, m_cubeMeshAsset);

	//m_twoCubeMeshAsset = std::make_shared<MeshAsset>("data/meshes/two_cubes.glb");
	//m_twoCubeMesh = std::make_shared<Mesh>(m_device, m_twoCubeMeshAsset);

	// m_sceneMeshAsset = std::make_shared<MeshAsset>("data/meshes/scene1.glb");
	// m_sceneMesh = std::make_shared<Mesh>(m_device, m_sceneMeshAsset);

	m_simpleVertexAsset = std::make_shared<ShaderAsset>(L"data/shaders/simple_vs.hlsl", "VSMain", "vs_5_0");
	m_simplePixelAsset = std::make_shared<ShaderAsset>(L"data/shaders/simple_ps.hlsl", "PSMain", "ps_5_0");

	std::unique_ptr<ID3DInclude> includer = std::make_unique<ShaderIncluder>();
	m_shaderCompiler = std::make_unique<ShaderCompiler>(std::move(includer));

	m_shaderCompiler->CompileShaderAsset(m_simpleVertexAsset);
	m_shaderCompiler->CompileShaderAsset(m_simplePixelAsset);

	m_simpleVertex = std::make_shared<VertexShader>(m_device, m_simpleVertexAsset);
	m_simplePixel = std::make_shared<PixelShader>(m_device, m_simplePixelAsset);

	m_camera = std::make_shared<Camera>();

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
	if (auto res = m_device->CreateInputLayout(inputElementDescs, ARRAYSIZE(inputElementDescs), m_simpleVertexAsset->GetBlob()->GetBufferPointer(), m_simpleVertexAsset->GetBlob()->GetBufferSize(), &m_inputLayout); FAILED(res)) {
		DXERROR(res);
	}

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

DX11Context::~DX11Context()
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

void DX11Context::EnumAdapters(std::vector<ComPtr<IDXGIAdapter>>& outAdapters) {
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

DX11Context::ComPtr<IDXGIAdapter> DX11Context::PickAdapter(const std::vector<ComPtr<IDXGIAdapter>>& adapters) {
	spdlog::info("picking adapter");

	DXGI_ADAPTER_DESC desc;

	for (auto& adapter : adapters) {
		if (auto res = adapter->GetDesc(&desc); FAILED(res)) {
			DXERROR(res);
		}

		char description[128];
		memset(description, 0, 128);
		wcstombs_s(nullptr, description, desc.Description, 128);
		// wcstombs();

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
	ASSERT(adapters.size() > 0, "");
	// return first adapter for now
	return adapters[0];
}

void DX11Context::EnumOutputs(ComPtr<IDXGIAdapter> adapter, std::vector<ComPtr<IDXGIOutput>>& outOutputs) {
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

DX11Context::ComPtr<IDXGIOutput> DX11Context::PickOutput(const std::vector<ComPtr<IDXGIOutput>>& outputs) {
	spdlog::info("picking outputs");

	for (auto& output : outputs) {
		DXGI_OUTPUT_DESC desc;
		if (auto res = output->GetDesc(&desc); FAILED(res)) {
			DXERROR(res);
		}

		char deviceName[128];
		memset(deviceName, 0, 128);
		wcstombs_s(nullptr, deviceName, desc.DeviceName, 32);

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

void DX11Context::CreateDeviceAndContext(UINT createFlags) {
	ASSERT(m_selectedAdapter.Get() != nullptr, "");

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
		ARRLEN(requiredFeatureLevels),
		D3D11_SDK_VERSION,
		&m_device,
		&supportedFeatureLevel,
		&m_deviceContext
	);

	if (FAILED(res)) {
		DXERROR(res);
	}

	ASSERT(supportedFeatureLevel == D3D_FEATURE_LEVEL_11_0, "");
	spdlog::info("created device and device context");

#if _DEBUG
	m_device->QueryInterface(IID_PPV_ARGS(&m_debugInfoQueue));
#endif
}

void DX11Context::CreateSwapchain(GLFWwindow* window, u32 bufferCount) {
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

void DX11Context::ObtainSwapchainResources() {
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

void DX11Context::ReleaseSwapchainResources() {
	m_renderTargetView.Reset();
	spdlog::info("release swapchain resources");
}

void DX11Context::ResizeSwapchainResources(u32 width, u32 height) {
	m_deviceContext->Flush();
	ReleaseSwapchainResources();

	if(auto res = m_swapchain->ResizeBuffers(1, width, height, DXGI_FORMAT::DXGI_FORMAT_UNKNOWN, 0); FAILED(res)) {
		DXERROR(res);
	}

	spdlog::info("resized swapchain buffers");

	ObtainSwapchainResources();
}

void DX11Context::Render() {


	ImGui_ImplDX11_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();
	ImGui::ShowDemoWindow();


	// begin render
	const float clearColor[4] = {0.1f, 0.1f, 0.1f, 1.0f};
	m_deviceContext->ClearRenderTargetView(m_renderTargetView.Get(), clearColor);
	m_deviceContext->ClearDepthStencilView(m_depthStencilView.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);

	float time = static_cast<float>(glfwGetTime());
	float angle = DirectX::XMScalarSin(time);
	float moveX = 0.5f * DirectX::XMScalarSin(time * 2.0f);
	float moveY = 0.5f * DirectX::XMScalarSin(time * 2.0f);
	auto rotMatrix = DirectX::XMMatrixRotationY(angle);
	//auto rotMatrix = DirectX::XMMatrixTranslation(angle, 0, 0);
	//auto rotMatrix = DirectX::XMMatrixIdentity();

	auto transMatrix = DirectX::XMMatrixTranslation(moveX, moveY, 0);

	DirectX::XMMATRIX modelToWorld = DirectX::XMMatrixIdentity();
	modelToWorld = modelToWorld * rotMatrix;
	modelToWorld = modelToWorld * transMatrix;

	m_camera->transform.matrix = DirectX::XMMatrixTranslation(0, 0, -1);

	mat4 worldToCam = m_camera->GetView();
	mat4 CamToProjection = m_camera->GetProjection();

	D3D11_MAPPED_SUBRESOURCE subresource;
	m_deviceContext->Map(m_matrixBuffer.Get(), 0, D3D11_MAP::D3D11_MAP_WRITE_DISCARD, 0, &subresource);
	MatrixBuffer* data = reinterpret_cast<MatrixBuffer*>(subresource.pData);
	data->ModelToWorld = DirectX::XMMatrixTranspose(modelToWorld);
	data->WorldToView = DirectX::XMMatrixTranspose(worldToCam);
	data->ViewToProjection = DirectX::XMMatrixTranspose(CamToProjection);
	m_deviceContext->Unmap(m_matrixBuffer.Get(), 0);

	D3D11_MAPPED_SUBRESOURCE pointSubresource;
	m_deviceContext->Map(m_pointLightBuffer.Get(), 0, D3D11_MAP::D3D11_MAP_WRITE_DISCARD, 0, &pointSubresource);
	PointLightBuffer* plBuffer = reinterpret_cast<PointLightBuffer*>(pointSubresource.pData);
	plBuffer->Pos = DirectX::XMFLOAT3(0.0, 0.0f, 0);
	plBuffer->Col = DirectX::XMFLOAT3(1.0, 1.0, 1.0);
	m_deviceContext->Unmap(m_pointLightBuffer.Get(), 0);

	m_deviceContext->IASetVertexBuffers(
		0, 
		m_quadMesh->GetVertexBufferCount(), 
		m_quadMesh->GetVertexBuffer().GetAddressOf(), 
		m_quadMesh->GetVertexBufferStrides().data(), 
		m_quadMesh->GetVertexBufferOffsets().data());

	m_deviceContext->IASetIndexBuffer(m_quadMesh->GetIndexBuffer().Get(), m_quadMesh->GetIndexBufferFormat(), 0);

	m_deviceContext->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	
	m_deviceContext->VSSetShader(m_simpleVertex->Get(), nullptr, 0);
	m_deviceContext->VSSetConstantBuffers(0, 1, m_matrixBuffer.GetAddressOf());

	m_deviceContext->PSSetShader(m_simplePixel->Get(), nullptr, 0);
	m_deviceContext->PSSetShaderResources(0, 1, m_testSRV.GetAddressOf());
	m_deviceContext->PSSetSamplers(0, 1, m_testSamplerState.GetAddressOf());
	m_deviceContext->PSSetConstantBuffers(0, 1, m_pointLightBuffer.GetAddressOf());
	m_deviceContext->PSSetConstantBuffers(1, 1, m_matrixBuffer.GetAddressOf());

	m_deviceContext->OMSetRenderTargets(1, m_renderTargetView.GetAddressOf(), m_depthStencilView.Get());

	m_deviceContext->DrawIndexed(m_quadMesh->GetIndexCount(), 0, 0);

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

void DX11Context::Render(std::shared_ptr<StaticMesh> mesh, std::shared_ptr<VertexShader> shader)
{

}

void DX11Context::HandleResize(u32 width, u32 height)
{
	ResizeSwapchainResources(width, height);


	//m_depthStencilView.Reset();
	//m_depthStencilTexture.Reset();
	//m_device->CreateTexture2D()
}
