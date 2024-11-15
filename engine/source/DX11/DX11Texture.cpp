#include "DX11Texture.hpp"

DX11Texture::DX11Texture(ComPtr<ID3D11Device> device, const CreateInfo& info)
{
	Create(device, info);
}

void DX11Texture::Create(ComPtr<ID3D11Device> device, const CreateInfo& info)
{
	const int width = info.width;
	const int height = info.height;
	const int numComponents = info.numComponents;
	const byte* data = info.data;

	D3D11_TEXTURE2D_DESC testTextureDesc = {
		.Width = static_cast<UINT>(width),
		.Height = static_cast<UINT>(height),
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
		.pSysMem = static_cast<const void*>(data),
		.SysMemPitch = (u32)width * numComponents * sizeof(byte),
		.SysMemSlicePitch = 0,
	};

	if (auto res = device->CreateTexture2D(&testTextureDesc, &testSubresourceData, &m_texture); FAILED(res)) {
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

	if (auto res = device->CreateShaderResourceView(m_texture.Get(), &testTextureSRVDesc, &m_srv); FAILED(res)) {
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

	if (auto res = device->CreateSamplerState(&testSamplerStateDesc, &m_samplerState); FAILED(res)) {
		DXERROR(res);
	}
}
