#include "StaticMesh.hpp"

void StaticMesh::CreateBuffers()
{
    if(auto ma = m_meshAsset.lock(); ma != nullptr) {

		const std::vector<float3>& positions = ma->GetPositions();
		const std::vector<float3>& normals = ma->GetNormals();
		const std::vector<float3>& tangents = ma->GetTangents();
		const std::vector<float3>& colors = ma->GetColors();
		const std::vector<float2>& uv0s = ma->GetUV0s();
		const std::vector<float2>& uv1s = ma->GetUV1s();

        const std::vector<u32>& indices = ma->GetIndices();

		std::vector<VertexType> vertices;
		vertices.reserve(positions.size());

		for(int i = 0;i < positions.size(); ++i) {
			vertices.emplace_back(
				VertexType{
					.position = positions[i],
					.normal = normals[i],
					.color = colors[i],
					.uv0 = uv0s[i],
				}
			);
		}

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

        if (auto res = m_device->CreateBuffer(&vertBufferDesc, &vertexBufferInitData, &m_vertexBuffer); FAILED(res)) {
            DXERROR(res);
        }

        D3D11_BUFFER_DESC indexBufferDesc = {
            .ByteWidth = static_cast<UINT>(sizeof(u32) * indices.size()),
            .Usage = D3D11_USAGE_DEFAULT,
            .BindFlags = D3D11_BIND_INDEX_BUFFER,
            .CPUAccessFlags = 0,
            .MiscFlags = 0,
            .StructureByteStride = sizeof(u32),
        };

        D3D11_SUBRESOURCE_DATA indexBufferInitData = {
            .pSysMem = indices.data(),
            // these have no meaning for index buffers
            .SysMemPitch = 0,
            .SysMemSlicePitch = 0,
        };
        
        if (auto res = m_device->CreateBuffer(&indexBufferDesc, &indexBufferInitData, &m_indexBuffer); FAILED(res)) {
            DXERROR(res);
        }
    }
}