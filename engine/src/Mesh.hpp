#pragma once

#include "Math.hpp"

#include <d3d11.h>
#include <wrl.h>

class Mesh {
	template<typename T>
	using ComPtr = Microsoft::WRL::ComPtr<T>;

public:

	Mesh() {}
	virtual ~Mesh() {}

	struct Vertex {
		float3 position;
		float3 normal;
		float3 color;
		float2 uv0;
	};

	void Load();



private:
	// @TODO: SOA vertex stream?
	ComPtr<ID3D11Buffer> m_vertexBuffer;
	ComPtr<ID3D11Buffer> m_indexBuffer;
};

