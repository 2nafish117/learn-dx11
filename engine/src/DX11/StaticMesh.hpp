#pragma once

#include "Basic.hpp"
#include "Math.hpp"
#include "AssetSystem.hpp"

#include "DX11/DX11Context.hpp"

#include <d3d11.h>
#include <wrl.h>

class StaticMesh {
	template<typename T>
	using ComPtr = Microsoft::WRL::ComPtr<T>;

public:
	struct Vertex {
		float3 position;
		float3 normal;
		float3 color;
		float2 uv0;
	};

	// @TODO: this class should interleave the vertex attributes and give it as a singe vertex ?
	class VertexGenerator {
	public:
	private:
	};

	using VertexType = Vertex;

	StaticMesh(ComPtr<ID3D11Device> device, std::weak_ptr<MeshAsset> asset) 
		: m_device(device), m_meshAsset(asset)
	{
		CreateBuffers();
	}

	inline ComPtr<ID3D11Buffer> GetVertexBuffer() {
		return m_vertexBuffer;
	}

	inline u32 GetVertexBufferCount() {
		return 1;
	}

	inline std::array<u32, 1> GetVertexBufferOffsets() {
		return std::array<u32, 1> { 0 };
	}

	inline std::array<u32, 1> GetVertexBufferStrides() {
		return std::array<u32, 1> { sizeof(VertexType) };
	}

	inline ComPtr<ID3D11Buffer> GetIndexBuffer() {
		return m_indexBuffer;
	}

	inline DXGI_FORMAT GetIndexBufferFormat() {
		return DXGI_FORMAT_R32_UINT;
	}

	inline uint GetVertexCount() {
		if(auto ma = m_meshAsset.lock(); ma != nullptr) {
			return static_cast<uint>(ma->GetPositions().size());
		}

		return 0;
	}

	inline uint GetIndexCount() {
		if(auto ma = m_meshAsset.lock(); ma != nullptr) {
			return static_cast<uint>(ma->GetIndices().size());
		}

		return 0;
	}

private:

	void CreateBuffers();
	
	ComPtr<ID3D11Device> m_device;

	// @TODO: SOA vertex stream?
	ComPtr<ID3D11Buffer> m_vertexBuffer;
	ComPtr<ID3D11Buffer> m_indexBuffer;

	std::weak_ptr<MeshAsset> m_meshAsset;
};

