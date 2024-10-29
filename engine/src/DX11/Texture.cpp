#include "Texture.hpp"

Texture::Texture(ComPtr<ID3D11Device> device, std::weak_ptr<TextureAsset> asset)
: m_device(device)
{
	if(auto ta = asset.lock(); ta != nullptr) {
		const int width = ta->GetWidth();
		const int height = ta->GetHeight();
		const int numComponents = ta->GetNumComponents();
		const byte* data = ta->GetData();

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

		if(auto res = m_device->CreateTexture2D(&testTextureDesc, &testSubresourceData, &m_texture); FAILED(res)) {
			DXERROR(res);
		}
	}	
}
