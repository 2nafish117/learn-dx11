#pragma once

#include "Basic.hpp"
#include "Math.hpp"
#include "AssetSystem.hpp"

#include "DX11Context.hpp"

#include <d3d11.h>
#include <wrl.h>

class Texture {
	template<typename T>
	using ComPtr = Microsoft::WRL::ComPtr<T>;
public:

	Texture(ComPtr<ID3D11Device> device, std::weak_ptr<TextureAsset> asset);

	inline ComPtr<ID3D11Texture2D> GetTexture() {
		return m_texture;
	}

private:
	ComPtr<ID3D11Device> m_device;
	ComPtr<ID3D11Texture2D> m_texture;
};