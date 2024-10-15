#pragma once

#include <wrl.h>
#include <d3dcompiler.h>
#include <d3d11.h>

#include "Basic.hpp"
#include "RendererUtils.hpp"

class ShaderBase {
public:

protected:
    template<typename T>
    using ComPtr = Microsoft::WRL::ComPtr<T>;

    ComPtr<ID3DBlob> m_blob;
    eastl::string_view m_filePath;

    ShaderBase(ComPtr<ID3DBlob> blob, eastl::string_view filePath)
        : m_blob(blob), m_filePath(filePath) {}

};

class ShaderCompiler {
private:
    template<typename T>
    using ComPtr = Microsoft::WRL::ComPtr<T>;

public:
    ShaderCompiler(eastl::string_view target = "vs_5_0") 
        : m_target(target)
    {}

    ComPtr<ID3DBlob> CompileShader(eastl::wstring_view filePath, eastl::string_view entryFunc, D3D_SHADER_MACRO* defines = nullptr, ComPtr<ID3DInclude> include = nullptr) {
        assert(filePath.data());
        assert(entryFunc.data());

        ComPtr<ID3DBlob> bytecode;
        ComPtr<ID3DBlob> errors;

        UINT flags1 = D3DCOMPILE_WARNINGS_ARE_ERRORS | D3DCOMPILE_ENABLE_STRICTNESS;
        // something to do with fx files???
        UINT flags2 = 0;

        if (auto res = D3DCompileFromFile(filePath.data(), defines, include.Get(), entryFunc.data(), m_target.data(), flags1, flags2, &bytecode, &errors); FAILED(res)) {
            DXERROR(res);
            spdlog::error("shader compile error: {}", (const char*)errors->GetBufferPointer());
        }

        return bytecode;
    }

private:
    eastl::string_view m_target;
};

class VertexShader : public ShaderBase {
    using Base = ShaderBase;
public:
    VertexShader(ComPtr<ID3D11Device> device, ComPtr<ID3DBlob> blob, eastl::string_view filePath) 
        : Base(blob, filePath), m_device(device)
    {
        assert(m_blob);

        // @TODO: what is this?
        ID3D11ClassLinkage* linkage = nullptr;
        m_device->CreateVertexShader(m_blob->GetBufferPointer(), m_blob->GetBufferSize(), linkage, &m_shader);
    }

    void SetInputLayout(D3D11_INPUT_ELEMENT_DESC descs) {

        D3D11_INPUT_ELEMENT_DESC inputElementDescs[] = {
            {
                .SemanticName = "POSITION",
                .SemanticIndex = 0,
                .Format = DXGI_FORMAT::DXGI_FORMAT_R32G32B32_FLOAT,
                .InputSlot = 0,
                .AlignedByteOffset = 0,
                .InputSlotClass = D3D11_INPUT_CLASSIFICATION::D3D11_INPUT_PER_VERTEX_DATA,
                .InstanceDataStepRate = 0,
            },
            {
                .SemanticName = "NORMAL",
                .SemanticIndex = 0,
                .Format = DXGI_FORMAT::DXGI_FORMAT_R32G32B32_FLOAT,
                .InputSlot = 0,
                .AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT,
                .InputSlotClass = D3D11_INPUT_CLASSIFICATION::D3D11_INPUT_PER_VERTEX_DATA,
                .InstanceDataStepRate = 0,
            },
            {
                .SemanticName = "COLOR",
                .SemanticIndex = 0,
                .Format = DXGI_FORMAT::DXGI_FORMAT_R32G32B32_FLOAT,
                .InputSlot = 0,
                .AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT,
                .InputSlotClass = D3D11_INPUT_CLASSIFICATION::D3D11_INPUT_PER_VERTEX_DATA,
                .InstanceDataStepRate = 0,
            },
            {
                .SemanticName = "TEXCOORD",
                .SemanticIndex = 0,
                .Format = DXGI_FORMAT::DXGI_FORMAT_R32G32_FLOAT,
                .InputSlot = 0,
                .AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT,
                .InputSlotClass = D3D11_INPUT_CLASSIFICATION::D3D11_INPUT_PER_VERTEX_DATA,
                .InstanceDataStepRate = 0,
            }
        };

        ComPtr<ID3D11InputLayout> m_inputLayout;
	    m_device->CreateInputLayout(inputElementDescs, ARRAYSIZE(inputElementDescs), vertexBytecode->GetBufferPointer(), vertexBytecode->GetBufferSize(), &m_inputLayout);
    }

    // set buffer
    // set sampler
    // set texture
private:

    ComPtr<ID3D11Device> m_device;
	ComPtr<ID3D11VertexShader> m_shader;
    ComPtr<ID3D11InputLayout> m_inputLayout;
};

// class PixelShader : public ShaderBase {
// public:
//     PixelShader(eastl::string_view filePath);
//     PixelShader(ComPtr<ID3DBlob> blob);
//     virtual ~PixelShader() override;

// private:

// 	ComPtr<ID3D11VertexShader> m_shader;
// };