#pragma once

#include "Basic.hpp"
#include "Math.hpp"
#include "AssetSystem.hpp"

#include "DX11ContextUtils.hpp"

#include <d3d11.h>
#include <wrl.h>

class DX11Mesh {
	template<typename T>
	using ComPtr = Microsoft::WRL::ComPtr<T>;

public:
	struct Vertex {
		float3 position;
		float3 normal;
		float3 color;
		float2 uv0;
	};

	using VertexType = Vertex;

	struct CreateInfo {
		// number of elements in each of the attibute arrays
		size_t attributesCount = 0;

		// attribute arrays
		float3* positions = nullptr;
		float3* normals = nullptr;
		float3* tangents = nullptr;
		float3* colors = nullptr;
		float2* uv0s = nullptr;
		float2* uv1s = nullptr;

		size_t indicesCount = 0;
		u32* indices = nullptr;
	};

	DX11Mesh(ComPtr<ID3D11Device> device, const CreateInfo& info) 
	{
		Create(device, info);
	}

	void Create(ComPtr<ID3D11Device> device, const CreateInfo& info);

	inline ComPtr<ID3D11Buffer> GetVertexBuffer() {
		return m_vertexBuffer;
	}

	// @TODO: hardcoded
	inline u32 GetVertexBufferCount() {
		return 1;
	}

	// @TODO: hardcoded
	inline std::array<u32, 1> GetVertexBufferOffsets() {
		return std::array<u32, 1> { 0 };
	}

	// @TODO: hardcoded
	inline std::array<u32, 1> GetVertexBufferStrides() {
		return std::array<u32, 1> { sizeof(VertexType) };
	}

	inline ComPtr<ID3D11Buffer> GetIndexBuffer() {
		return m_indexBuffer;
	}

	// @TODO: hardcoded
	inline DXGI_FORMAT GetIndexBufferFormat() {
		return DXGI_FORMAT_R32_UINT;
	}

	inline uint GetVertexCount() { 
		return m_vertexCount;
	}

	inline uint GetIndexCount() { 
		return m_indexCount;
	}

private:

	// @TODO: SOA vertex stream?
	ComPtr<ID3D11Buffer> m_vertexBuffer;
	ComPtr<ID3D11Buffer> m_indexBuffer;

	uint m_vertexCount = 0;
	uint m_indexCount = 0;
};

