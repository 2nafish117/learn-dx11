#pragma once

#include "Basic.hpp"
#include "Math.hpp"
#include "AssetSystem.hpp"

#include "DX11ContextUtils.hpp"

#include <d3d11.h>
#include <wrl.h>

class DX11Texture {
	template<typename T>
	using ComPtr = Microsoft::WRL::ComPtr<T>;
public:

	struct CreateInfo {
		int width;
		int height;
		int numComponents;

		// @TODO: more stuff here

		byte* data;
	};

	DX11Texture(ComPtr<ID3D11Device> device, const CreateInfo& asset);

	void Create(ComPtr<ID3D11Device> device, const CreateInfo& asset);

	inline ID3D11Texture2D* Get() {
		return m_texture.Get();
	}

	inline ComPtr<ID3D11ShaderResourceView> GetSRV() {
		return m_srv;
	}

	inline ComPtr<ID3D11SamplerState> GetSamplerState() {
		return m_samplerState;
	}

	/*inline ID3D11ShaderResourceView* GetSRV() {
		return m_srv.Get();
	}*/

private:
	ComPtr<ID3D11Texture2D> m_texture;
	ComPtr<ID3D11ShaderResourceView> m_srv;

	// @TODO: get this from sampler state cache?
	ComPtr<ID3D11SamplerState> m_samplerState;
};