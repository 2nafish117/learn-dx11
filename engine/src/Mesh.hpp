#pragma once

#include "Basic.hpp"
#include "Math.hpp"

#include "Renderer/DX11ContextUtils.hpp"

#include <d3d11.h>
#include <wrl.h>

#include <cgltf/cgltf.h>

class MeshAsset {

public:
	struct Vertex {
		float3 position;
		float3 normal;
		float3 color;
		float2 uv0;
	};

	MeshAsset(std::string_view filePath);
	MeshAsset(const std::vector<Vertex>& vertices, const std::vector<u32>& indices) ;

	inline const std::vector<Vertex>& GetVertices() {
		return m_vertices;
	}

	inline const std::vector<u32>& GetIndices() {
		return m_indices;
	}

private:
	void GltfPrintInfo(cgltf_data* data);

private:
	std::string_view m_filePath;
	// @TODO: store vertices in SOA
	std::vector<Vertex> m_vertices;
	std::vector<u32> m_indices;
};


class Mesh {
	template<typename T>
	using ComPtr = Microsoft::WRL::ComPtr<T>;

	using VertexType = MeshAsset::Vertex;

public:

	Mesh(ComPtr<ID3D11Device> device, std::weak_ptr<MeshAsset> asset) 
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

	inline size_t GetVertexCount() {
		if(auto ma = m_meshAsset.lock(); ma != nullptr) {
			return ma->GetVertices().size();
		}

		return 0;
	}

	inline size_t GetIndexCount() {
		if(auto ma = m_meshAsset.lock(); ma != nullptr) {
			return ma->GetIndices().size();
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

