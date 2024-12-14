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

	std::vector<DXGI_MODE_DESC> modeDescs;
	GetOutputModes(modeDescs);

	spdlog::info("available display modes:");
	for (const auto& desc : modeDescs) {
		spdlog::info("width={} height={} refreshRate={}/{}, format={} scanlineOrdering={} scaling={}", 
			desc.Width, desc.Height, desc.RefreshRate.Numerator, desc.RefreshRate.Denominator, (uint)desc.Format, (uint)desc.ScanlineOrdering, (uint)desc.Scaling);
	}

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
	UINT flags = 0;
	if (auto res = DXGIGetDebugInterface1(flags, IID_PPV_ARGS(&m_dxgiDebug)); FAILED(res)) {
		DXERROR(res);
	}
#endif
	// triple buffering
	CreateSwapchain(m_window, 3);
	ObtainSwapchainResources();

	// @TODO: query device for supported texture formats
	//m_device->CheckFormatSupport();

	int width = 0, height = 0;
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

	D3D11_RASTERIZER_DESC rasterDesc2 = {
		.FillMode = D3D11_FILL_SOLID,
		.CullMode = D3D11_CULL_BACK,
		.FrontCounterClockwise = false,
		.DepthBias = 0,
		.DepthBiasClamp = 0.0f,
		.SlopeScaledDepthBias = 0.0f,
		.DepthClipEnable = false,
		.ScissorEnable = false,
		.MultisampleEnable = false,
		.AntialiasedLineEnable = false,
	};

	// Create the rasterizer state from the description we just filled out.
	if (auto res = m_device->CreateRasterizerState(&rasterDesc2, &m_rasterState2); FAILED(res)) {
		spdlog::critical("create rasterizer state failed with: {}", res);
	}

	m_viewport = {
		.TopLeftX = 0.0f,
		.TopLeftY = 0.0f,
		.Width = static_cast<float>(width),
		.Height = static_cast<float>(height),
		.MinDepth = 0.0f,
		.MaxDepth = 1.0f,
	};

	shaderCompiler = std::make_unique<ShaderCompiler>(new ShaderIncluder());

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

	CreateGbuffer(static_cast<uint>(width), static_cast<uint>(height));

	InitImgui();
}

void DX11Context::CreateGbuffer(uint width, uint height)
{
	// @TODO: optimise to use 16 bit float textures for normal, and albedo
	//albedo stuff
	{
		D3D11_TEXTURE2D_DESC albedoDesc = {
				.Width = width,
				.Height = height,
				.MipLevels = 1,
				.ArraySize = 1,
				.Format = DXGI_FORMAT::DXGI_FORMAT_R32G32B32A32_FLOAT,
				.SampleDesc = {
					.Count = 1,
					.Quality = 0,
				},
				.Usage = D3D11_USAGE::D3D11_USAGE_DEFAULT,
				.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE,
				.CPUAccessFlags = 0,
				.MiscFlags = 0,
		};

		// @TODO: DXERROR will not print if an api call crashes sometimes, do the DXCALL macro stuff i guess?
		if (auto res = m_device->CreateTexture2D(&albedoDesc, nullptr, &m_gbufferData.albedoTexture); FAILED(res)) {
			DXERROR(res);
		}

		D3D11_RENDER_TARGET_VIEW_DESC albedoRTVDesc = {
			.Format = DXGI_FORMAT::DXGI_FORMAT_R32G32B32A32_FLOAT,
			.ViewDimension = D3D11_RTV_DIMENSION::D3D11_RTV_DIMENSION_TEXTURE2D,
			.Texture2D = {
				.MipSlice = 0,
			},
		};

		if (auto res = m_device->CreateRenderTargetView(m_gbufferData.albedoTexture.Get(), &albedoRTVDesc, &m_gbufferData.albedoRTV); FAILED(res)) {
			DXERROR(res);
		}

		D3D11_SHADER_RESOURCE_VIEW_DESC albedoSRVDesc = {
			.Format = DXGI_FORMAT::DXGI_FORMAT_R32G32B32A32_FLOAT,
			.ViewDimension = D3D11_SRV_DIMENSION::D3D11_SRV_DIMENSION_TEXTURE2D,
			.Texture2D = {
				.MostDetailedMip = 0,
				.MipLevels = static_cast<UINT>(-1),
			},
		};

		if (auto res = m_device->CreateShaderResourceView(m_gbufferData.albedoTexture.Get(), &albedoSRVDesc, &m_gbufferData.albedoSRV); FAILED(res)) {
			DXERROR(res);
		}
	}

	// position stuff
	{
		D3D11_TEXTURE2D_DESC positionDesc = {
			.Width = width,
			.Height = height,
			.MipLevels = 1,
			.ArraySize = 1,
			.Format = DXGI_FORMAT::DXGI_FORMAT_R32G32B32A32_FLOAT,
			.SampleDesc = {
				.Count = 1,
				.Quality = 0,
			},
			.Usage = D3D11_USAGE::D3D11_USAGE_DEFAULT,
			.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE,
			.CPUAccessFlags = 0,
			.MiscFlags = 0,
		};

		// @TODO: DXERROR will not print if an api call crashes sometimes, do the DXCALL macro stuff i guess?
		if (auto res = m_device->CreateTexture2D(&positionDesc, nullptr, &m_gbufferData.wsPositionTexture); FAILED(res)) {
			DXERROR(res);
		}

		D3D11_RENDER_TARGET_VIEW_DESC positionRTVDesc = {
			.Format = DXGI_FORMAT::DXGI_FORMAT_R32G32B32A32_FLOAT,
			.ViewDimension = D3D11_RTV_DIMENSION::D3D11_RTV_DIMENSION_TEXTURE2D,
			.Texture2D = {
				.MipSlice = 0,
			},
		};

		if (auto res = m_device->CreateRenderTargetView(m_gbufferData.wsPositionTexture.Get(), &positionRTVDesc, &m_gbufferData.wsPositionRTV); FAILED(res)) {
			DXERROR(res);
		}

		D3D11_SHADER_RESOURCE_VIEW_DESC positionSRVDesc = {
			.Format = DXGI_FORMAT::DXGI_FORMAT_R32G32B32A32_FLOAT,
			.ViewDimension = D3D11_SRV_DIMENSION::D3D11_SRV_DIMENSION_TEXTURE2D,
			.Texture2D = {
				.MostDetailedMip = 0,
				.MipLevels = static_cast<UINT>(-1),
			},
		};

		if (auto res = m_device->CreateShaderResourceView(m_gbufferData.wsPositionTexture.Get(), &positionSRVDesc, &m_gbufferData.wsPositionSRV); FAILED(res)) {
			DXERROR(res);
		}
	}

	// normal stuff
	{		
		D3D11_TEXTURE2D_DESC normalDesc = {
			.Width = width,
			.Height = height,
			.MipLevels = 1,
			.ArraySize = 1,
			.Format = DXGI_FORMAT::DXGI_FORMAT_R32G32B32A32_FLOAT,
			.SampleDesc = {
				.Count = 1,
				.Quality = 0,
			},
			.Usage = D3D11_USAGE::D3D11_USAGE_DEFAULT,
			.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE,
			.CPUAccessFlags = 0,
			.MiscFlags = 0,
		};

		if (auto res = m_device->CreateTexture2D(&normalDesc, nullptr, &m_gbufferData.wsNormalTexture); FAILED(res)) {
			DXERROR(res);
		}

		D3D11_RENDER_TARGET_VIEW_DESC normalRTVDesc = {
			.Format = DXGI_FORMAT::DXGI_FORMAT_R32G32B32A32_FLOAT,
			.ViewDimension = D3D11_RTV_DIMENSION::D3D11_RTV_DIMENSION_TEXTURE2D,
			.Texture2D = {
				.MipSlice = 0
			},
		};

		if (auto res = m_device->CreateRenderTargetView(m_gbufferData.wsNormalTexture.Get(), &normalRTVDesc, &m_gbufferData.wsNormalRTV); FAILED(res)) {
			DXERROR(res);
		}

		D3D11_SHADER_RESOURCE_VIEW_DESC normalSRVDesc = {
			.Format = DXGI_FORMAT::DXGI_FORMAT_R32G32B32A32_FLOAT,
			.ViewDimension = D3D11_SRV_DIMENSION::D3D11_SRV_DIMENSION_TEXTURE2D,
			.Texture2D = {
				.MostDetailedMip = 0,
				.MipLevels = static_cast<UINT>(-1),
			},
		};

		if (auto res = m_device->CreateShaderResourceView(m_gbufferData.wsNormalTexture.Get(), &normalSRVDesc, &m_gbufferData.wsNormalSRV); FAILED(res)) {
			DXERROR(res);
		}
	}
}

DX11Context::~DX11Context()
{
	// @TODO: make a renderer system that uses dx11 context internally?
	spdlog::info("Renderer System de-init");

#ifdef DX11_DEBUG
	// @TODO: this is kinda stupid because the lifetimes of the dx objects are tied to the lifetimes of the renderer
	// and running this at in the destructor of renderer means they havent been destroyed yet
	// so it will report that all objects are still alive, where should i run this then? pull it out of the renderer?
	ReportLiveDeviceObjects();
#endif

	ImGui_ImplDX11_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
}

void DX11Context::Render(const RuntimeScene& scene)
{
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	ImGui::ShowDemoWindow();
	// ImGui::Begin("the name", nullptr, ImGuiWindowFlags_DockNodeHost);

	// ImGui::End();

	// begin render

	m_deviceContext->ClearState();

	m_deviceContext->RSSetViewports(1, &m_viewport);
	m_deviceContext->RSSetState(m_rasterState.Get());
	m_deviceContext->OMSetDepthStencilState(m_depthStencilState.Get(), 0);


	// @TODO: can i just clear just before the first draw indexed?
	const float clearColor[4] = { 0.2f, 0.1f, 0.1f, 1.0f };
	m_deviceContext->ClearRenderTargetView(m_renderTargetView.Get(), clearColor);



	const float clearBlackColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	m_deviceContext->ClearRenderTargetView(m_gbufferData.albedoRTV.Get(), clearBlackColor);
	m_deviceContext->ClearRenderTargetView(m_gbufferData.wsPositionRTV.Get(), clearBlackColor);
	m_deviceContext->ClearRenderTargetView(m_gbufferData.wsNormalRTV.Get(), clearBlackColor);

	m_deviceContext->ClearDepthStencilView(m_depthStencilView.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);

	float time = static_cast<float>(glfwGetTime());
	float angle = DirectX::XMScalarSin(time);
	float moveX = 0.5f * DirectX::XMScalarSin(time * 2.0f);
	float moveY = 0.5f * DirectX::XMScalarSin(time * 2.0f);
	auto rotMatrix = DirectX::XMMatrixRotationY(angle + DirectX::XM_PI);

	auto transMatrix = DirectX::XMMatrixTranslation(moveX, moveY, 0);

	DirectX::XMMATRIX modelToWorld = scene.staticMeshEntity0->xform.matrix;
	modelToWorld = modelToWorld * rotMatrix;
	modelToWorld = modelToWorld * transMatrix;

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

	// @TODO: move this to DX11Mesh?
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
	DX11VertexShader* vertShaderSimple = (DX11VertexShader*)vertShaderAssetSimple.GetRendererResource();
	const ShaderAsset& pixShaderAssetSimple = global::assetSystem->Catalog()->GetShaderAsset(scene.staticMeshEntity0->pixShaderAsset);
	DX11PixelShader* pixShaderSimple = (DX11PixelShader*)pixShaderAssetSimple.GetRendererResource();

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
	DX11Mesh* rendererMesh = (DX11Mesh*)meshAsset0.GetRendererResource();

	const TextureAsset& texAsset = global::assetSystem->Catalog()->GetTextureAsset(scene.staticMeshEntity0->texAsset);
	DX11Texture* texture = (DX11Texture*)texAsset.GetRendererResource();

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

	ID3D11RenderTargetView* renderTargets[] = { m_gbufferData.albedoRTV.Get(), m_gbufferData.wsPositionRTV.Get(), m_gbufferData.wsNormalRTV.Get() };

	//@TODO: render ws_position, ws_normal, albedo, ...
	m_deviceContext->OMSetRenderTargets(ARRLEN(renderTargets), renderTargets, m_depthStencilView.Get());
	m_deviceContext->RSSetState(m_rasterState.Get());
	m_deviceContext->DrawIndexed(rendererMesh->GetIndexCount(), 0, 0);

	// final pass

	// // @TODO: move this to DX11Mesh?
	//D3D11_INPUT_ELEMENT_DESC finalPassInputElementDescs[] = {
	//	{
	//		.SemanticName = "POSITION",
	//		.SemanticIndex = 0,
	//		.Format = DXGI_FORMAT::DXGI_FORMAT_R32G32B32_FLOAT,
	//		.InputSlot = 0,
	//		.AlignedByteOffset = 0,
	//		.InputSlotClass = D3D11_INPUT_CLASSIFICATION::D3D11_INPUT_PER_VERTEX_DATA,
	//		.InstanceDataStepRate = 0,
	//	},
	//	{
	//		.SemanticName = "TEXCOORD",
	//		.SemanticIndex = 0,
	//		.Format = DXGI_FORMAT::DXGI_FORMAT_R32G32_FLOAT,
	//		.InputSlot = 0,
	//		.AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT,
	//		.InputSlotClass = D3D11_INPUT_CLASSIFICATION::D3D11_INPUT_PER_VERTEX_DATA,
	//		.InstanceDataStepRate = 0,
	//	}
	//};


	const ShaderAsset& vertShaderAssetFinalPass = global::assetSystem->Catalog()->GetShaderAsset(m_finalPassVertexShader);
	DX11VertexShader* vertShaderFinalPass = (DX11VertexShader*)vertShaderAssetFinalPass.GetRendererResource();
	const ShaderAsset& pixShaderAssetFinalPass = global::assetSystem->Catalog()->GetShaderAsset(m_finalPassPixelShader);
	DX11PixelShader* pixShaderFinalPass = (DX11PixelShader*)pixShaderAssetFinalPass.GetRendererResource();

	//ComPtr<ID3D11InputLayout> m_finalPassinputLayout;
	//if (auto res = m_device->CreateInputLayout(
	//	finalPassInputElementDescs,
	//	ARRAYSIZE(finalPassInputElementDescs),
	//	vertShaderAssetFinalPass.blob,
	//	vertShaderAssetFinalPass.blobSize,
	//	&m_finalPassinputLayout); FAILED(res))
	//{
	//	DXERROR(res);
	//}

	//m_deviceContext->IASetInputLayout(m_finalPassinputLayout.Get());

	const MeshAsset& quadMesh = global::assetSystem->Catalog()->GetMeshAsset(m_quadMesh);
	DX11Mesh* rendererQuadMesh = (DX11Mesh*)quadMesh.GetRendererResource();

	m_deviceContext->ClearRenderTargetView(m_renderTargetView.Get(), clearColor);

	//m_deviceContext->ClearDepthStencilView(m_depthStencilView.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);

	// @TODO: do i have to add null to the end like this to unbind prev render targets in those slots??
	ID3D11RenderTargetView* renderTargetsFinal[] = { m_renderTargetView.Get(), nullptr, nullptr };

	m_deviceContext->OMSetRenderTargets(ARRLEN(renderTargetsFinal), renderTargetsFinal, m_depthStencilView.Get());

	m_deviceContext->IASetVertexBuffers(
		0,
		rendererQuadMesh->GetVertexBufferCount(),
		rendererQuadMesh->GetVertexBuffer().GetAddressOf(),
		rendererQuadMesh->GetVertexBufferStrides().data(),
		rendererQuadMesh->GetVertexBufferOffsets().data());

	m_deviceContext->IASetIndexBuffer(rendererQuadMesh->GetIndexBuffer().Get(), rendererQuadMesh->GetIndexBufferFormat(), 0);
	m_deviceContext->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	m_deviceContext->VSSetShader(vertShaderFinalPass->Get(), nullptr, 0);
	m_deviceContext->PSSetShader(pixShaderFinalPass->Get(), nullptr, 0);

	m_deviceContext->PSSetShaderResources(0, 1, m_gbufferData.albedoSRV.GetAddressOf());
	m_deviceContext->PSSetShaderResources(1, 1, m_gbufferData.wsPositionSRV.GetAddressOf());
	m_deviceContext->PSSetShaderResources(2, 1, m_gbufferData.wsNormalSRV.GetAddressOf());

	m_deviceContext->PSSetSamplers(0, 1, texture->GetSamplerState().GetAddressOf());

	m_deviceContext->PSSetConstantBuffers(0, 1, m_pointLightBuffer.GetAddressOf());
	m_deviceContext->PSSetConstantBuffers(1, 1, m_matrixBuffer.GetAddressOf());

	// disable depth clip
	//m_deviceContext->RSSetState(m_rasterState2.Get());

	// @TODO: final quad isnt being drawn!!!!
	m_deviceContext->DrawIndexed(rendererQuadMesh->GetIndexCount(), 0, 0);

	// // end final pass

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

void DX11Context::GetOutputModes(std::vector<DXGI_MODE_DESC>& outOutputModes)
{
	UINT numModes = 0;
	m_selectedOutput->GetDisplayModeList(
		DXGI_FORMAT::DXGI_FORMAT_R8G8B8A8_UNORM,
		DXGI_ENUM_MODES_INTERLACED,
		&numModes,
		nullptr);

	outOutputModes.resize(numModes);

	m_selectedOutput->GetDisplayModeList(
		DXGI_FORMAT::DXGI_FORMAT_R8G8B8A8_UNORM,
		DXGI_ENUM_MODES_INTERLACED,
		&numModes,
		outOutputModes.data());
}

void DX11Context::CreateDeviceAndContext(UINT createFlags) {
	// @TODO: https://walbourn.github.io/anatomy-of-direct3d-11-create-device/
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

#define IMGUI_STYLE
#ifdef IMGUI_STYLE
	bool bStyleDark_ = true;
	float alpha_ = 0.9f;

	ImGuiStyle& style = ImGui::GetStyle();

	// light style from Pac�me Danhiez (user itamago) https://github.com/ocornut/imgui/pull/511#issuecomment-175719267
	style.Alpha = 1.0f;
	style.FrameRounding = 3.0f;
	style.Colors[ImGuiCol_Text] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
	style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
	style.Colors[ImGuiCol_WindowBg] = ImVec4(0.94f, 0.94f, 0.94f, 0.94f);
	//style.Colors[ImGuiCol_ChildWindowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	style.Colors[ImGuiCol_PopupBg] = ImVec4(1.00f, 1.00f, 1.00f, 0.94f);
	style.Colors[ImGuiCol_Border] = ImVec4(0.00f, 0.00f, 0.00f, 0.39f);
	style.Colors[ImGuiCol_BorderShadow] = ImVec4(1.00f, 1.00f, 1.00f, 0.10f);
	style.Colors[ImGuiCol_FrameBg] = ImVec4(1.00f, 1.00f, 1.00f, 0.94f);
	style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.40f);
	style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
	style.Colors[ImGuiCol_TitleBg] = ImVec4(0.96f, 0.96f, 0.96f, 1.00f);
	style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(1.00f, 1.00f, 1.00f, 0.51f);
	style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.82f, 0.82f, 0.82f, 1.00f);
	style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.86f, 0.86f, 0.86f, 1.00f);
	style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.98f, 0.98f, 0.98f, 0.53f);
	style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.69f, 0.69f, 0.69f, 1.00f);
	style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.59f, 0.59f, 0.59f, 1.00f);
	style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.49f, 0.49f, 0.49f, 1.00f);
	//style.Colors[ImGuiCol_ComboBg] = ImVec4(0.86f, 0.86f, 0.86f, 0.99f);
	style.Colors[ImGuiCol_CheckMark] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
	style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.24f, 0.52f, 0.88f, 1.00f);
	style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
	style.Colors[ImGuiCol_Button] = ImVec4(0.26f, 0.59f, 0.98f, 0.40f);
	style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
	style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.06f, 0.53f, 0.98f, 1.00f);
	style.Colors[ImGuiCol_Header] = ImVec4(0.26f, 0.59f, 0.98f, 0.31f);
	style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
	style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
	//style.Colors[ImGuiCol_Column] = ImVec4(0.39f, 0.39f, 0.39f, 1.00f);
	//style.Colors[ImGuiCol_ColumnHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.78f);
	//style.Colors[ImGuiCol_ColumnActive] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
	style.Colors[ImGuiCol_ResizeGrip] = ImVec4(0.26f, 0.59f, 0.98f, 0.37f);
	style.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
	style.Colors[ImGuiCol_ResizeGripActive] = ImVec4(0.26f, 0.59f, 0.98f, 0.95f);
	//style.Colors[ImGuiCol_CloseButton] = ImVec4(0.59f, 0.59f, 0.59f, 0.50f);
	//style.Colors[ImGuiCol_CloseButtonHovered] = ImVec4(0.98f, 0.39f, 0.36f, 1.00f);
	//style.Colors[ImGuiCol_CloseButtonActive] = ImVec4(0.98f, 0.39f, 0.36f, 1.00f);
	style.Colors[ImGuiCol_PlotLines] = ImVec4(0.39f, 0.39f, 0.39f, 1.00f);
	style.Colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
	style.Colors[ImGuiCol_PlotHistogram] = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
	style.Colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
	style.Colors[ImGuiCol_TextSelectedBg] = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
	//style.Colors[ImGuiCol_ModalWindowDarkening] = ImVec4(0.20f, 0.20f, 0.20f, 0.35f);

	if (bStyleDark_)
	{
		for (int i = 0; i <= ImGuiCol_COUNT; i++)
		{
			ImVec4& col = style.Colors[i];
			float H, S, V;
			ImGui::ColorConvertRGBtoHSV(col.x, col.y, col.z, H, S, V);

			if (S < 0.1f)
			{
				V = 1.0f - V;
			}
			ImGui::ColorConvertHSVtoRGB(H, S, V, col.x, col.y, col.z);
			if (col.w < 1.00f)
			{
				col.w *= alpha_;
			}
		}
	}
	else
	{
		for (int i = 0; i <= ImGuiCol_COUNT; i++)
		{
			ImVec4& col = style.Colors[i];
			if (col.w < 1.00f)
			{
				col.x *= alpha_;
				col.y *= alpha_;
				col.z *= alpha_;
				col.w *= alpha_;
			}
		}
	}
#endif
#undef IMGUI_STYLE

	ImGui_ImplGlfw_InitForOther(m_window, true);
	ImGui_ImplDX11_Init(GetDevice().Get(), GetDeviceContext().Get());
}