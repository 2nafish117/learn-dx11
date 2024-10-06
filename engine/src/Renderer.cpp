#include "Renderer.hpp"

#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

Renderer::Renderer(GLFWwindow* window)
	: m_window(window)
{
	if(auto res = CreateDXGIFactory2(D3D11_CREATE_DEVICE_DEBUG, IID_PPV_ARGS(&m_factory)); FAILED(res)) {
		spdlog::error("CreateDXGIFactory2 err with {}", res);
		return;
	}
	
	// @TODO: select adapter
	//m_adapters.reserve(4);
	//for(int i = 0; i < 128; ++i)
	//{
	//	ComPtr<IDXGIAdapter> adapter;
	//	HRESULT res = m_factory->EnumAdapters(i, &adapter);
	//	
	//	if (res == DXGI_ERROR_NOT_FOUND) {
	//		break;
	//	}

	//	m_adapters.push_back(adapter);
	//}

	//for (auto& ad : m_adapters) {
	//}

	//m_factory->EnumAdapters(0, &m_adapters[0]);

	// @TODO: error check
	//DXGI_ADAPTER_DESC desc;
	//m_adapters[0]->GetDesc(&desc);

	//spdlog::info("{}", desc);
	
	//ComPtr<IDXGIOutput> output;
	//m_adapters[0]->EnumOutputs(0, &output);

	//DXGI_OUTPUT_DESC outputDesc;
	//output->GetDesc(&outputDesc);


	//spdlog::info(L"got output {}", outputDesc.DeviceName);

	//HMODULE theModule = GetModuleHandleW(nullptr);
	//HMODULE theModule = GetModuleHandle(nullptr);
	HWND hwnd = glfwGetWin32Window(m_window);
	D3D_FEATURE_LEVEL featureLevels[] = {D3D_FEATURE_LEVEL_11_1};
	
	int width, height;
	glfwGetWindowSize(m_window, &width, &height);

	DXGI_SWAP_CHAIN_DESC swapChainDesc = {
		.BufferDesc = {
	 		.Width = (UINT)width,
	 		.Height = (UINT)height,
	 		.RefreshRate = {
	 			.Numerator = 0,
	 			.Denominator = 1,
	 		},
	 		.Format = DXGI_FORMAT_R8G8B8A8_UNORM,
	 		.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED,
	 		.Scaling = DXGI_MODE_SCALING_UNSPECIFIED,
		},
		.SampleDesc = {
	 		.Count = 1,
	 		.Quality = 0,
		},
		.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
		.BufferCount = 1,
		.OutputWindow = hwnd,
		.Windowed = true,
		.SwapEffect = DXGI_SWAP_EFFECT_DISCARD,
		.Flags = 0,
	};
	
	
	// @TODO: error handle
	// HRESULT res = D3D11CreateDevice(
	// 	nullptr,
	// 	D3D_DRIVER_TYPE::D3D_DRIVER_TYPE_HARDWARE,
	// 	theModule, 
	// 	D3D11_CREATE_DEVICE_DEBUG,
	// 	featureLevels, 1,
	// 	D3D11_SDK_VERSION,
	// 	&m_device,
	// 	nullptr,
	// 	&m_deviceContext
	// );

	//m_factory->CreateSwapChainForHwnd()

	// @TODO: err handle
	if (auto res = D3D11CreateDeviceAndSwapChain(
		nullptr,
		D3D_DRIVER_TYPE_HARDWARE,
		nullptr,
		0,
		featureLevels, 1,
		D3D11_SDK_VERSION,
		&swapChainDesc,
		&m_swapchain,
		&m_device,
		nullptr,
		&m_deviceContext
	); FAILED(res)) {
		spdlog::critical("D3D11CreateDeviceAndSwapChain failed with {}", res);
		return;
	}
	
	spdlog::info("created device context and swapchain");

	ComPtr<ID3D11Texture2D> backBuffer;
	if (auto res = m_swapchain->GetBuffer(0, IID_PPV_ARGS(&backBuffer)); FAILED(res)) {
		spdlog::critical("get buffer failed with {}", res);
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
		spdlog::critical("CreateRenderTargetView failed with {}", res);
		return;
	}

	spdlog::info("created render target view");

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
		spdlog::critical("depth stencil texture creation failed with {}", res);
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
		spdlog::error("buffer creation failed with: {}", res);
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
		spdlog::error("buffer creation failed with: {}", res);
	}

	D3D11_INPUT_ELEMENT_DESC CreateInputLayout = {

	};
	// m_device->CreateInputLayout(&inputElementDesc, 3, )
}

Renderer::~Renderer()
{
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

namespace shit {
	using namespace std;
}