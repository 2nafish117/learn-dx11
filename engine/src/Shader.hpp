#pragma once

#include <wrl.h>
#include <d3dcompiler.h>
#include <d3d11.h>
#include <d3dcommon.h>
#include <dxgi1_6.h>

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
    ShaderCompiler(std::unique_ptr<ID3DInclude>&& includer = nullptr) 
        : m_includer(std::move(includer))
    {}

    ComPtr<ID3DBlob> CompileShader(eastl::wstring_view filePath, eastl::string_view entryFunc, eastl::string_view target, D3D_SHADER_MACRO* defines = nullptr);

private:
	std::unique_ptr<ID3DInclude> m_includer;
};

// @TODO: implement the shader includer
class ShaderIncluder : public ID3DInclude {

public:
	HRESULT Open(D3D_INCLUDE_TYPE IncludeType, LPCSTR pFileName, LPCVOID pParentData, LPCVOID *ppData, UINT *pBytes);
	HRESULT Close(LPCVOID pData);
};

class VertexShader : public ShaderBase {
    using Base = ShaderBase;
public:
    VertexShader(ComPtr<ID3D11Device> device, ComPtr<ID3DBlob> blob, eastl::string_view filePath);

	inline ComPtr<ID3D11VertexShader> Get() {
		return m_shader;
	}
private:

    ComPtr<ID3D11Device> m_device;
	ComPtr<ID3D11VertexShader> m_shader;
	// @TODO: do we need a ref to the input layout ?
    // ComPtr<ID3D11InputLayout> m_inputLayout;
};

class PixelShader : public ShaderBase {
    using Base = ShaderBase;
public:
    PixelShader(ComPtr<ID3D11Device> device, ComPtr<ID3DBlob> blob, eastl::string_view filePath);

	inline ComPtr<ID3D11PixelShader> Get() {
		return m_shader;
	}
private:

    ComPtr<ID3D11Device> m_device;
	ComPtr<ID3D11PixelShader> m_shader;
};
