#pragma once

#include <wrl.h>
#include <dxgi1_6.h>
#include <d3d11.h>

#include "Basic.hpp"
#include "DX11ContextUtils.hpp"
#include "AssetSystem.hpp"

class DX11ShaderBase {
public:

protected:
    template<typename T>
    using ComPtr = Microsoft::WRL::ComPtr<T>;

    DX11ShaderBase(ComPtr<ID3D11Device> device) {}
};

class ShaderCompiler {
private:
    template<typename T>
    using ComPtr = Microsoft::WRL::ComPtr<T>;

public:
    ShaderCompiler(ID3DInclude* includer = nullptr) 
        : m_includer(includer)
    {}

	~ShaderCompiler() {
		if(m_includer != nullptr) {
			// let it leak, who cares, atleast im not dealing with undefined behaviour
			// delete m_includer;
		}
	}

    bool CompileShaderAsset(ShaderID asset);

private:
	ID3DInclude* m_includer;

	struct CompiledResult {
		ComPtr<ID3DBlob> blob;
		ComPtr<ID3DBlob> error;
	};
	CompiledResult CompileShader(
		std::wstring_view filePath, 
		std::string_view entryFunc, 
		std::string_view target, 
		const std::vector<D3D_SHADER_MACRO>& defines);
};

// @TODO: implement the shader includer
class ShaderIncluder : public ID3DInclude {

public:
	HRESULT Open(D3D_INCLUDE_TYPE IncludeType, LPCSTR pFileName, LPCVOID pParentData, LPCVOID *ppData, UINT *pBytes);
	HRESULT Close(LPCVOID pData);
};


class DX11VertexShader : public DX11ShaderBase {
    using Base = DX11ShaderBase;

public:

	struct CreateInfo {
		const byte* blob = nullptr;
		size_t blobSize = 0;
	};
    
	DX11VertexShader(ComPtr<ID3D11Device> device, const CreateInfo& info);


	inline ID3D11VertexShader* Get() {
		return m_shader.Get();
	}

private:
	void Create(ComPtr<ID3D11Device> device, const CreateInfo& info);
	
	ComPtr<ID3D11VertexShader> m_shader;

	// @TODO: do we need a ref to the input layout ?
    // ComPtr<ID3D11InputLayout> m_inputLayout;
};


class DX11PixelShader : public DX11ShaderBase {
    using Base = DX11ShaderBase;

public:

	struct CreateInfo {
		const byte* blob = nullptr;
		size_t blobSize = 0;
	};

    DX11PixelShader(ComPtr<ID3D11Device> device, const CreateInfo& info);

	inline ID3D11PixelShader* Get() {
		return m_shader.Get();
	}
private:
	void Create(ComPtr<ID3D11Device> device, const CreateInfo& info);

	ComPtr<ID3D11PixelShader> m_shader;
};

