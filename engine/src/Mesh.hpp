#pragma once

#include "Basic.hpp"
#include "Math.hpp"

#include "Renderer/DX11ContextUtils.hpp"

#include <d3d11.h>
#include <wrl.h>

struct cgltf_data;

class MeshAsset {

public:
	// @TODO: temporary
	MeshAsset(std::string_view filePath);
	MeshAsset(
		const std::vector<float3>& positions,
		const std::vector<float3>& normals, 
		const std::vector<float3>& tangents,
		const std::vector<float3>& colors,
		const std::vector<float2>& uv0s,
		const std::vector<float2>& uv1s,
		const std::vector<u32>& indices);

	inline const std::vector<float3>& GetPositions() {
		return m_positions;
	}

	inline const std::vector<float3>& GetNormals() {
		return m_normals;
	}

	inline const std::vector<float3>& GetTangents() {
		return m_tangents;
	}

	inline const std::vector<float3>& GetColors() {
		return m_colors;
	}

	inline const std::vector<float2>& GetUV0s() {
		return m_uv0s;
	}

	inline const std::vector<float2>& GetUV1s() {
		return m_uv1s;
	}

	inline const std::vector<u32>& GetIndices() {
		return m_indices;
	}

private:
	void GltfPrintInfo(cgltf_data* data);

	void GltfPrintMeshInfo(cgltf_data* data);
	void GltfPrintAnimationInfo(cgltf_data* data);
	void GltfPrintMaterialInfo(cgltf_data* data);
	void GltfPrintImageInfo(cgltf_data* data);

private:
	std::string_view m_filePath;
	// @TODO: store vertices in SOA
	//std::vector<Vertex> m_vertices;
	
	std::vector<u32> m_indices;

	std::vector<float3> m_positions;
	std::vector<float3> m_normals;
	std::vector<float3> m_tangents;
	std::vector<float3> m_colors;
	std::vector<float2> m_uv0s;
	std::vector<float2> m_uv1s;
};

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

