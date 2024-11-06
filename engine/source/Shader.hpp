#pragma once

#include <wrl.h>
#include <dxgi1_6.h>
#include <d3d11.h>

#include "Basic.hpp"
#include "DX11/DX11ContextUtils.hpp"

struct ShaderMacro
{
	const char* name;
	const char* definition;
};

class ShaderAsset {

public:
	ShaderAsset(std::wstring_view filePath, std::string_view entryFunc, std::string_view target, const std::vector<ShaderMacro>& defines = {})
		: m_filePath(filePath), m_entryFunc(entryFunc), m_target(target), m_defines(defines) {}

	inline std::wstring_view GetFilePath() {
		return m_filePath;
	}

	inline std::string_view GetEntryFunc() {
		return m_entryFunc;
	}

	inline std::string_view GetTarget() {
		return m_target;
	}

	inline const std::vector<ShaderMacro>& GetDefines() {
		return m_defines;
	}

public:
	const byte* blob = nullptr;
	size_t blobSize = 0;

private:
    std::wstring_view m_filePath = L"";
	std::string_view m_entryFunc = "";
	std::string_view m_target = "";
	std::vector<ShaderMacro> m_defines;
};


class ShaderBase {
public:

protected:
    template<typename T>
    using ComPtr = Microsoft::WRL::ComPtr<T>;

	std::weak_ptr<ShaderAsset> m_shaderAsset;
	ComPtr<ID3D11Device> m_device;

    ShaderBase(ComPtr<ID3D11Device> device, std::weak_ptr<ShaderAsset> asset)
        : m_device(device), m_shaderAsset(asset) {}
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

    bool CompileShaderAsset(std::weak_ptr<ShaderAsset> asset);

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
		const std::vector<ShaderMacro>& defines);
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
    VertexShader(ComPtr<ID3D11Device> device, std::weak_ptr<ShaderAsset> asset);

	inline ID3D11VertexShader* Get() {
		return m_shader.Get();
	}

private:
	ComPtr<ID3D11VertexShader> m_shader;
	// @TODO: do we need a ref to the input layout ?
    // ComPtr<ID3D11InputLayout> m_inputLayout;
};


class PixelShader : public ShaderBase {
    using Base = ShaderBase;

public:
    PixelShader(ComPtr<ID3D11Device> device, std::weak_ptr<ShaderAsset> asset);

	inline ID3D11PixelShader* Get() {
		return m_shader.Get();
	}
private:

	ComPtr<ID3D11PixelShader> m_shader;
};

