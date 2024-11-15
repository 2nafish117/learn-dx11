#include "DX11Context.hpp"

#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include <imgui.h>
#include <backends/imgui_impl_dx11.h>
#include <backends/imgui_impl_glfw.h>

#include <comdef.h>

#include "DX11Mesh.hpp"
#include "DX11Texture.hpp"
#include "DX11Shader.hpp"

#include "SceneSystem.hpp"
#include "AssetSystem.hpp"


DX11Context::DX11Context(GLFWwindow* window)
	: m_window(window)
{
	m_scratchSize = 4096;
	m_scratchMemory = new byte[m_scratchSize];

	ASSERT(m_window != nullptr, "");

	UINT factoryCreateFlags = 0;

// @TODO: make a DXDEBUG flag
#ifdef DX11_DEBUG
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
#ifdef DX11_DEBUG
	deviceCreateFlags |= D3D11_CREATE_DEVICE_FLAG::D3D11_CREATE_DEVICE_DEBUG;
#endif

	CreateDeviceAndContext(deviceCreateFlags);

#ifdef DX11_DEBUG
	if (auto res = m_device->QueryInterface(IID_PPV_ARGS(&m_debug)); FAILED(res)) {
		DXERROR(res);
	}

	// @TODO: flags?
	if (auto res = DXGIGetDebugInterface1(0, IID_PPV_ARGS(&m_dxgiDebug)); FAILED(res)) {
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

	//std::vector<float3> positions = {
	//	float3(-0.5f, -0.5f, 0.5f), float3(0.5f, 0.5f, 0.5f), float3(0.5f, -0.5f, 0.5f), float3(-0.5f, 0.5f, 0.5f)
	//};
	//std::vector<float3> normals = {
	//	float3(0.0f, 0.0f, -1.0f),
	//	float3(0.0f, 0.0f, -1.0f),
	//	float3(0.0f, 0.0f, -1.0f),
	//	float3(0.0f, 0.0f, -1.0f),
	//};
	//std::vector<float3> tangents;
	//std::vector<float3> colors = {
	//	float3(0.0f, 0.5f, 0.0f),
	//	float3(0.0f, 0.0f, 0.5f),
	//	float3(0.0f, 0.0f, 0.5f),
	//	float3(0.5f, 0.0f, 0.0f),
	//};
	//std::vector<float2> uv0s = {
	//	float2(0.0f, 1.0f),
	//	float2(1.0f, 0.0f),
	//	float2(1.0f, 1.0f),
	//	float2(0.0f, 0.0f),
	//};
	//std::vector<float2> uv1s;

	//std::vector<u32> indices = {
	//	0, 1, 2,
	//	0, 3, 1
	//};

	//m_quadMeshAsset = std::make_shared<MeshAsset>(positions, normals, tangents, colors, uv0s, uv1s, indices);
	//m_quadMesh = std::make_shared<DX11Mesh>(m_device, m_quadMeshAsset);

	//m_cubeMeshAsset = std::make_shared<MeshAsset>("meshes/suzanne.glb");
	//m_cubeMeshAsset->Load();
	//m_cubeMesh = std::make_shared<DX11Mesh>(m_device, m_cubeMeshAsset);

	//m_twoCubeMeshAsset = std::make_shared<MeshAsset>("meshes/two_cubes.glb");
	//m_twoCubeMesh = std::make_shared<Mesh>(m_device, m_twoCubeMeshAsset);

	// m_sceneMeshAsset = std::make_shared<MeshAsset>("meshes/scene1.glb");
	// m_sceneMesh = std::make_shared<Mesh>(m_device, m_sceneMeshAsset);

	//m_simpleVertexAsset = std::make_shared<ShaderAsset>(L"shaders/simple_vs.hlsl", "VSMain", "vs_5_0");
	//m_simplePixelAsset = std::make_shared<ShaderAsset>(L"shaders/simple_ps.hlsl", "PSMain", "ps_5_0");

	shaderCompiler = std::make_unique<ShaderCompiler>(new ShaderIncluder());

	//m_shaderCompiler->CompileShaderAsset(m_simpleVertexAsset);
	//m_shaderCompiler->CompileShaderAsset(m_simplePixelAsset);

	//m_simpleVertex = std::make_shared<VertexShader>(m_device, m_simpleVertexAsset);
	//m_simplePixel = std::make_shared<PixelShader>(m_device, m_simplePixelAsset);

	// m_camera = std::make_shared<CameraEntity>();

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

	//ComPtr<ID3D11InputLayout> m_inputLayout;
	//if (auto res = m_device->CreateInputLayout(inputElementDescs, ARRAYSIZE(inputElementDescs), m_simpleVertexAsset->blob, m_simpleVertexAsset->blobSize, &m_inputLayout); FAILED(res)) {
	//	DXERROR(res);
	//}

	//m_deviceContext->IASetInputLayout(m_inputLayout.Get());

	//m_testTexAsset = std::make_shared<TextureAsset>("textures/checker.png");

	//std::unique_ptr<Texture> m_testTexture = std::make_unique<Texture>(m_device, m_testTexAsset);

	//D3D11_SHADER_RESOURCE_VIEW_DESC testTextureSRVDesc = {
	//	.Format = DXGI_FORMAT::DXGI_FORMAT_R8G8B8A8_UNORM,
	//	.ViewDimension = D3D11_SRV_DIMENSION::D3D11_SRV_DIMENSION_TEXTURE2D,
	//	.Texture2D = {
	//		.MostDetailedMip = 0,
	//		.MipLevels = static_cast<UINT>(-1),
	//	}
	//};

	//if(auto res = m_device->CreateShaderResourceView(m_testTexture->GetTexture().Get(), &testTextureSRVDesc, &m_testSRV); FAILED(res)) {
	//	DXERROR(res);
	//}

	//D3D11_SAMPLER_DESC testSamplerStateDesc = {
	//	.Filter = D3D11_FILTER::D3D11_FILTER_MIN_MAG_MIP_LINEAR,
	//	.AddressU = D3D11_TEXTURE_ADDRESS_MODE::D3D11_TEXTURE_ADDRESS_WRAP,
	//	.AddressV = D3D11_TEXTURE_ADDRESS_MODE::D3D11_TEXTURE_ADDRESS_WRAP,
	//	.AddressW = D3D11_TEXTURE_ADDRESS_MODE::D3D11_TEXTURE_ADDRESS_WRAP,
	//	.MipLODBias = 0.0f,
	//	.MaxAnisotropy = 1,
	//	.ComparisonFunc = D3D11_COMPARISON_FUNC::D3D11_COMPARISON_NEVER,
	//	.BorderColor = {0, 0, 0, 0},
	//	.MinLOD = 0.0f,
	//	.MaxLOD = D3D11_FLOAT32_MAX,
	//};

	//if(auto res = m_device->CreateSamplerState(&testSamplerStateDesc, &m_testSamplerState); FAILED(res)) {
	//	DXERROR(res);
	//}

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

	InitImgui();
}

DX11Context::~DX11Context()
{
	// @TODO: make a renderer system that uses dx11 context internally?
	spdlog::info("Renderer System de-init");

#ifdef DX11_DEBUG
	// @TODO: this is kinda stupid because the lifetimes of the dx objects are tied to the lifetimes of the renderer
	// and running this at in the destructor of renderer means they havent been destroyed yet
	// so it will report that all objects are still alive, where should i run this then? pull it out of the renderer?
	if (auto res = m_debug->ReportLiveDeviceObjects(D3D11_RLDO_FLAGS::D3D11_RLDO_DETAIL); FAILED(res)) {
		DXERROR(res);
	}

	// @TODO: is this different from above code?
	//m_dxgiDebug->ReportLiveObjects()

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

	if (auto res = D3D11CreateDevice(
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
	); FAILED(res)) {
		DXERROR(res);
	}

	ASSERT(supportedFeatureLevel == D3D_FEATURE_LEVEL_11_0, "");
	spdlog::info("created device and device context");

#ifdef DX11_DEBUG
	if(auto res = m_device->QueryInterface(IID_PPV_ARGS(&m_debugInfoQueue)); FAILED(res)) {
		DXERROR(res);
	}
#endif
	if(auto res = m_deviceContext->QueryInterface(IID_PPV_ARGS(&m_annotation)); FAILED(res)) {
		DXERROR(res);
	}
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

# if 0
void DX11Context::Render() {

	ImGui_ImplDX11_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	ImGui::ShowDemoWindow();

	// ImGui::Begin("the name", nullptr, ImGuiWindowFlags_DockNodeHost);
	
	// ImGui::End();

	// begin render
	const float clearColor[4] = {0.2f, 0.1f, 0.1f, 1.0f};
	m_deviceContext->ClearRenderTargetView(m_renderTargetView.Get(), clearColor);
	m_deviceContext->ClearDepthStencilView(m_depthStencilView.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);

	float time = static_cast<float>(glfwGetTime());
	float angle = DirectX::XMScalarSin(time);
	float moveX = 0.5f * DirectX::XMScalarSin(time * 2.0f);
	float moveY = 0.5f * DirectX::XMScalarSin(time * 2.0f);
	auto rotMatrix = DirectX::XMMatrixRotationY(angle + DirectX::XM_PI);
	//auto rotMatrix = DirectX::XMMatrixTranslation(angle, 0, 0);
	//auto rotMatrix = DirectX::XMMatrixIdentity();

	auto transMatrix = DirectX::XMMatrixTranslation(moveX, moveY, 0);

	DirectX::XMMATRIX modelToWorld = DirectX::XMMatrixIdentity();
	modelToWorld = modelToWorld * rotMatrix;
	modelToWorld = modelToWorld * transMatrix;

	// m_camera->transform.matrix = DirectX::XMMatrixTranslation(0, 0, -3);

	// mat4 worldToCam = m_camera->GetView();
	// mat4 CamToProjection = m_camera->GetProjection();

	mat4 worldToCam = DirectX::XMMatrixIdentity();
	mat4 CamToProjection = DirectX::XMMatrixIdentity();

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
	plBuffer->Pos = DirectX::XMFLOAT3(1.0, 1.0f, -3);
	plBuffer->Col = DirectX::XMFLOAT3(1.0, 1.0, 1.0);
	m_deviceContext->Unmap(m_pointLightBuffer.Get(), 0);

	// draw mesh 1
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

	// draw mesh 2
	m_deviceContext->IASetVertexBuffers(
		0, 
		m_cubeMesh->GetVertexBufferCount(), 
		m_cubeMesh->GetVertexBuffer().GetAddressOf(), 
		m_cubeMesh->GetVertexBufferStrides().data(), 
		m_cubeMesh->GetVertexBufferOffsets().data());
	
	m_deviceContext->IASetIndexBuffer(m_cubeMesh->GetIndexBuffer().Get(), m_cubeMesh->GetIndexBufferFormat(), 0);

	m_deviceContext->DrawIndexed(m_cubeMesh->GetIndexCount(), 0, 0);

	m_annotation->BeginEvent(L"ImGui");
	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

	// Update and Render additional Platform Windows
	// if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault();
	}

	m_annotation->EndEvent();

	// end render
	// vsync enabled
	m_swapchain->Present(0, 0);

#ifdef DX11_DEBUG
	LogDebugInfo();
#endif
}
#endif


void DX11Context::Render(const RuntimeScene& scene)
{
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	ImGui::ShowDemoWindow();
	// ImGui::Begin("the name", nullptr, ImGuiWindowFlags_DockNodeHost);
	
	// ImGui::End();


	// begin render
	const float clearColor[4] = {0.2f, 0.1f, 0.1f, 1.0f};
	m_deviceContext->ClearRenderTargetView(m_renderTargetView.Get(), clearColor);
	m_deviceContext->ClearDepthStencilView(m_depthStencilView.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);

	float time = static_cast<float>(glfwGetTime());
	float angle = DirectX::XMScalarSin(time);
	float moveX = 0.5f * DirectX::XMScalarSin(time * 2.0f);
	float moveY = 0.5f * DirectX::XMScalarSin(time * 2.0f);
	auto rotMatrix = DirectX::XMMatrixRotationY(angle + DirectX::XM_PI);
	//auto rotMatrix = DirectX::XMMatrixTranslation(angle, 0, 0);
	//auto rotMatrix = DirectX::XMMatrixIdentity();

	auto transMatrix = DirectX::XMMatrixTranslation(moveX, moveY, 0);

	DirectX::XMMATRIX modelToWorld = DirectX::XMMatrixIdentity();
	modelToWorld = modelToWorld * rotMatrix;
	modelToWorld = modelToWorld * transMatrix;

	scene.camera->xform.matrix = DirectX::XMMatrixTranslation(0, 0, -3);

	mat4 worldToCam = scene.camera->GetView();
	mat4 CamToProjection = scene.camera->GetProjection();

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
	plBuffer->Pos = DirectX::XMFLOAT3(1.0, 1.0f, -3);
	plBuffer->Col = DirectX::XMFLOAT3(1.0, 1.0, 1.0);
	m_deviceContext->Unmap(m_pointLightBuffer.Get(), 0);


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

	const ShaderAsset& vertShaderAssetSimple = global::assetSystem->Catalog()->GetShaderAsset(scene.staticMeshEntity0->vertShaderAsset);
	DX11VertexShader* vertShaderSimple = (DX11VertexShader*) vertShaderAssetSimple.GetRendererResource();
	const ShaderAsset& pixShaderAssetSimple = global::assetSystem->Catalog()->GetShaderAsset(scene.staticMeshEntity0->pixShaderAsset);
	DX11PixelShader* pixShaderSimple = (DX11PixelShader*) pixShaderAssetSimple.GetRendererResource();

	ComPtr<ID3D11InputLayout> m_inputLayout;
	if (auto res = m_device->CreateInputLayout(
		inputElementDescs, 
		ARRAYSIZE(inputElementDescs), 
		vertShaderAssetSimple.blob,
		vertShaderAssetSimple.blobSize,
		&m_inputLayout); FAILED(res)) 
	{
		DXERROR(res);
	}

	m_deviceContext->IASetInputLayout(m_inputLayout.Get());

	const MeshAsset& meshAsset0 = global::assetSystem->Catalog()->GetMeshAsset(scene.staticMeshEntity0->meshAsset);
	DX11Mesh* rendererMesh = (DX11Mesh*) meshAsset0.GetRendererResource();
	
	const TextureAsset& texAsset = global::assetSystem->Catalog()->GetTextureAsset(scene.staticMeshEntity0->texAsset);
	DX11Texture* texture = (DX11Texture*) texAsset.GetRendererResource();

	// draw mesh 1
	m_deviceContext->IASetVertexBuffers(
		0, 
		rendererMesh->GetVertexBufferCount(),
		rendererMesh->GetVertexBuffer().GetAddressOf(),
		rendererMesh->GetVertexBufferStrides().data(),
		rendererMesh->GetVertexBufferOffsets().data());

	m_deviceContext->IASetIndexBuffer(rendererMesh->GetIndexBuffer().Get(), rendererMesh->GetIndexBufferFormat(), 0);

	m_deviceContext->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	
	m_deviceContext->VSSetShader(vertShaderSimple->Get(), nullptr, 0);
	m_deviceContext->VSSetConstantBuffers(0, 1, m_matrixBuffer.GetAddressOf());

	m_deviceContext->PSSetShader(pixShaderSimple->Get(), nullptr, 0);
	m_deviceContext->PSSetShaderResources(0, 1, texture->GetSRV().GetAddressOf());
	m_deviceContext->PSSetSamplers(0, 1, texture->GetSamplerState().GetAddressOf());
	m_deviceContext->PSSetConstantBuffers(0, 1, m_pointLightBuffer.GetAddressOf());
	m_deviceContext->PSSetConstantBuffers(1, 1, m_matrixBuffer.GetAddressOf());

	m_deviceContext->OMSetRenderTargets(1, m_renderTargetView.GetAddressOf(), m_depthStencilView.Get());

	m_deviceContext->DrawIndexed(rendererMesh->GetIndexCount(), 0, 0);

#ifdef DX11_DEBUG
	// log errors from our code
	LogDebugInfo();
#endif

	m_annotation->BeginEvent(L"ImGui");
	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

	// Update and Render additional Platform Windows
	// if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault();
	}

	m_annotation->EndEvent();

#ifdef DX11_DEBUG
	// log errors from imgui
	LogDebugInfo();
#endif

	VerifyGraphicsPipeline();

	// end render
	// vsync enabled
	if (auto res = m_swapchain->Present(0, 0); FAILED(res)) {
		DXERROR(res);
	}
}

void DX11Context::HandleResize(u32 width, u32 height)
{
	ResizeSwapchainResources(width, height);

	//m_depthStencilView.Reset();
	//m_depthStencilTexture.Reset();
	//m_device->CreateTexture2D()
}

void DX11Context::InitImgui() {
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // IF using Docking Branch
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

	// imgui font
	{
		ImFontConfig font_config = {};

		font_config.FontDataOwnedByAtlas = true;
		font_config.OversampleH = 6;
		font_config.OversampleV = 6;
		font_config.GlyphMaxAdvanceX = std::numeric_limits<float>::max();
		font_config.RasterizerMultiply = 1.4f;
		font_config.RasterizerDensity = 1.0f;
		font_config.EllipsisChar = std::numeric_limits<u16>::max();

		font_config.PixelSnapH = false;
		font_config.GlyphOffset = ImVec2{0.0, -1.0};

		io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/segoeui.ttf", 18.0f, &font_config);

		font_config.MergeMode = true;

		const u16 ICON_MIN_FA = 0xe005;
		const u16 ICON_MAX_FA = 0xf8ff;

		static u16 FA_RANGES[3] = {ICON_MIN_FA, ICON_MAX_FA, 0};

		font_config.RasterizerMultiply = 1.0;
		font_config.GlyphOffset = ImVec2{0.0, -1.0};

		// @TODO: need more fonts?
		// io.Fonts->AddFontFromFileTTF("assets/fa-regular-400.ttf", 14.0, &font_config, FA_RANGES);

		font_config.MergeMode = false;
	}

	// style imgui
	// @TODO: not highlight state on buttons with this style, fix.
	if(0)
	{
		ImGuiStyle& style = ImGui::GetStyle();

		const ImVec4 tone_text_1 = {0.69f, 0.69f, 0.69f, 1.0f};
		const ImVec4 tone_text_2 = {0.69f, 0.69f, 0.69f, 0.8f};

		ImVec4 tone_1 = {0.16f, 0.16f, 0.28f, 1.0f};
		ImVec4 tone_1_b = {tone_1.x * 1.2f, tone_1.y * 1.2f, tone_1.z * 1.2f, tone_1.w * 1.2f};
		ImVec4 tone_1_e = {tone_1.x * 1.2f, tone_1.y * 1.2f, tone_1.z * 1.2f, tone_1.w * 1.2f};
		ImVec4 tone_1_e_a = tone_1_e;
		ImVec4 tone_3 = {0.11f, 0.11f, 0.18f, 1.0};
		ImVec4 tone_2 = tone_3;
		ImVec4 tone_2_b = tone_2;

		style.Colors[ImGuiCol_Text] = tone_text_1;
		style.Colors[ImGuiCol_TextDisabled] = tone_text_2;
		style.Colors[ImGuiCol_WindowBg] = tone_1;
		style.Colors[ImGuiCol_ChildBg] = tone_2;
		style.Colors[ImGuiCol_PopupBg] = tone_2_b;
		style.Colors[ImGuiCol_Border] = tone_2;
		style.Colors[ImGuiCol_BorderShadow] = {0.0, 0.0, 0.0, 0.0};
		style.Colors[ImGuiCol_FrameBg] = tone_3;
		style.Colors[ImGuiCol_FrameBgHovered] = tone_3;
		style.Colors[ImGuiCol_FrameBgActive] = tone_3;
		style.Colors[ImGuiCol_TitleBg] = tone_2;
		style.Colors[ImGuiCol_TitleBgActive] = tone_2;
		style.Colors[ImGuiCol_TitleBgCollapsed] = tone_2;
		style.Colors[ImGuiCol_MenuBarBg] = tone_2;
		style.Colors[ImGuiCol_ScrollbarBg] = tone_3;
		style.Colors[ImGuiCol_ScrollbarGrab] = tone_1_e;
		style.Colors[ImGuiCol_ScrollbarGrabHovered] = tone_1_e;
		style.Colors[ImGuiCol_ScrollbarGrabActive] = tone_1_e_a;
		style.Colors[ImGuiCol_CheckMark] = tone_1_e;
		style.Colors[ImGuiCol_SliderGrab] = tone_1_e;
		style.Colors[ImGuiCol_SliderGrabActive] = tone_1_e_a;
		style.Colors[ImGuiCol_Button] = tone_2;
		style.Colors[ImGuiCol_ButtonHovered] = tone_2;
		style.Colors[ImGuiCol_ButtonActive] = tone_3;
		style.Colors[ImGuiCol_Header] = tone_2;
		style.Colors[ImGuiCol_HeaderHovered] = tone_2;
		style.Colors[ImGuiCol_HeaderActive] = tone_2;
		style.Colors[ImGuiCol_Separator] = tone_2;
		style.Colors[ImGuiCol_SeparatorHovered] = tone_2;
		style.Colors[ImGuiCol_SeparatorActive] = tone_2;
		style.Colors[ImGuiCol_ResizeGrip] = {0.0, 0.0, 0.0, 0.0};
		style.Colors[ImGuiCol_ResizeGripHovered] = {0.0, 0.0, 0.0, 0.0};
		style.Colors[ImGuiCol_ResizeGripActive] = {0.0, 0.0, 0.0, 0.0};
		style.Colors[ImGuiCol_Tab] = tone_2;
		style.Colors[ImGuiCol_TabHovered] = tone_1;
		style.Colors[ImGuiCol_TabActive] = tone_1;
		style.Colors[ImGuiCol_TabUnfocused] = tone_1;
		style.Colors[ImGuiCol_TabUnfocusedActive] = tone_1;
		style.Colors[ImGuiCol_PlotLines] = tone_1_e;
		style.Colors[ImGuiCol_PlotLinesHovered] = tone_2;
		style.Colors[ImGuiCol_PlotHistogram] = tone_1_e;
		style.Colors[ImGuiCol_PlotHistogramHovered] = tone_2;
		style.Colors[ImGuiCol_TableHeaderBg] = tone_2;
		style.Colors[ImGuiCol_TableBorderStrong] = tone_2;
		style.Colors[ImGuiCol_TableBorderLight] = tone_2;
		style.Colors[ImGuiCol_TableRowBg] = tone_2;
		style.Colors[ImGuiCol_TableRowBgAlt] = tone_1;
		style.Colors[ImGuiCol_TextSelectedBg] = tone_1_e;
		style.Colors[ImGuiCol_DragDropTarget] = tone_2;
		style.Colors[ImGuiCol_NavHighlight] = tone_2;
		style.Colors[ImGuiCol_NavWindowingHighlight] = tone_2;
		style.Colors[ImGuiCol_NavWindowingDimBg] = tone_2_b;
		style.Colors[ImGuiCol_ModalWindowDimBg] = {tone_2_b.x * 0.5f, tone_2_b.y * 0.5f, tone_2_b.z * 0.5f, tone_2_b.w * 0.5f};

		style.Colors[ImGuiCol_DockingPreview] = {1.0, 1.0, 1.0, 0.5};
		style.Colors[ImGuiCol_DockingEmptyBg] = {0.0, 0.0, 0.0, 0.0};

		style.WindowPadding = {10.00, 10.00};
		style.FramePadding = {5.00, 5.00};
		style.CellPadding = {2.50, 2.50};
		style.ItemSpacing = {5.00, 5.00};
		style.ItemInnerSpacing = {5.00, 5.00};
		style.TouchExtraPadding = {5.00, 5.00};
		style.IndentSpacing = 10;
		style.ScrollbarSize = 15;
		style.GrabMinSize = 10;
		style.WindowBorderSize = 0;
		style.ChildBorderSize = 0;
		style.PopupBorderSize = 0;
		style.FrameBorderSize = 0;
		style.TabBorderSize = 0;
		style.WindowRounding = 10;
		style.ChildRounding = 5;
		style.FrameRounding = 5;
		style.PopupRounding = 5;
		style.GrabRounding = 5;
		style.ScrollbarRounding = 10;
		style.LogSliderDeadzone = 5;
		style.TabRounding = 5;
		style.DockingSeparatorSize = 5;
	}


	ImGui_ImplGlfw_InitForOther(m_window, true);
	ImGui_ImplDX11_Init(GetDevice().Get(), GetDeviceContext().Get());
}