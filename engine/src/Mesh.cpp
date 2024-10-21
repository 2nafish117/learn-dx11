#include "Mesh.hpp"

void Mesh::CreateBuffers()
{
    if(auto ma = m_meshAsset.lock(); ma != nullptr) {

        const std::vector<VertexType>& vertices = ma->GetVertices();
        const std::vector<u32>& indices = ma->GetIndices();

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