#include "DX11Mesh.hpp"

void DX11Mesh::Create(ComPtr<ID3D11Device> device, const CreateInfo& info)
{
	std::vector<VertexType> vertices;
	vertices.reserve(info.attributesCount);

	for (int i = 0; i < info.attributesCount; ++i) {
		vertices.emplace_back(
			VertexType{
				.position = info.positions[i],
				.normal = info.normals[i],
				.color = info.colors[i],
				.uv0 = info.uv0s[i],
			}
		);
	}

	m_vertexCount = vertices.size();
	m_indexCount = info.indicesCount;

	D3D11_BUFFER_DESC vertBufferDesc = {
		.ByteWidth = static_cast<UINT>(sizeof(VertexType) * vertices.size()),
		.Usage = D3D11_USAGE_DEFAULT,
		.BindFlags = D3D11_BIND_VERTEX_BUFFER,
		.CPUAccessFlags = 0,
		.MiscFlags = 0,
		.StructureByteStride = sizeof(VertexType),
	};

	D3D11_SUBRESOURCE_DATA vertexBufferInitData = {
		.pSysMem = vertices.data(),
		// these have no meaning for vertex buffers
		.SysMemPitch = 0,
		.SysMemSlicePitch = 0,
	};

	if (auto res = device->CreateBuffer(&vertBufferDesc, &vertexBufferInitData, &m_vertexBuffer); FAILED(res)) {
		DXERROR(res);
	}

	D3D11_BUFFER_DESC indexBufferDesc = {
		.ByteWidth = static_cast<UINT>(sizeof(u32) * info.indicesCount),
		.Usage = D3D11_USAGE_DEFAULT,
		.BindFlags = D3D11_BIND_INDEX_BUFFER,
		.CPUAccessFlags = 0,
		.MiscFlags = 0,
		.StructureByteStride = sizeof(u32),
	};

	D3D11_SUBRESOURCE_DATA indexBufferInitData = {
		.pSysMem = info.indices,
		// these have no meaning for index buffers
		.SysMemPitch = 0,
		.SysMemSlicePitch = 0,
	};

	if (auto res = device->CreateBuffer(&indexBufferDesc, &indexBufferInitData, &m_indexBuffer); FAILED(res)) {
		DXERROR(res);
	}
}