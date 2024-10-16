#pragma once

#include "Math.hpp"

#include <d3d11.h>
#include <wrl.h>
// #include <dxgi1_6.h>
// #include <d3dcompiler.h>

class Mesh {
public:
	struct Vertex {
		float3 position;
		float3 normal;
		float3 color;
		float2 uv0;
	};


private:
	template<typename T>
	using ComPtr = Microsoft::WRL::ComPtr<T>;

	// @TODO: SOA vertex stream
	ComPtr<ID3D11Buffer> m_vertexBuffer;
	ComPtr<ID3D11Buffer> m_indexBuffer;
};