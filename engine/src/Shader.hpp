#pragma once

#include <wrl.h>
#include <d3dcompiler.h>
#include <d3d11.h>
#include <d3dcommon.h>
#include <dxgi1_6.h>

#include "Basic.hpp"
#include "RendererUtils.hpp"


class ShaderAsset {
	template<typename T>
    using ComPtr = Microsoft::WRL::ComPtr<T>;

public:
    ShaderAsset(eastl::wstring_view filePath, eastl::string_view entryFunc, eastl::string_view target, const std::vector<D3D_SHADER_MACRO>& defines = {})
        : m_filePath(filePath), m_entryFunc(entryFunc), m_target(target), m_defines(defines) {}

	inline bool IsCompiled() {
		return m_blob != nullptr;
	}

	inline ComPtr<ID3DBlob> GetBlob() {
		return m_blob;
	}

	inline void SetBlob(ComPtr<ID3DBlob> blob) {
		m_blob = blob;
	}

	inline eastl::wstring_view GetFilePath() {
		return m_filePath;
	}

	inline eastl::string_view GetEntryFunc() {
		return m_entryFunc;
	}

	inline eastl::string_view GetTarget() {
		return m_target;
	}

	inline const std::vector<D3D_SHADER_MACRO>& GetDefines() {
		return m_defines;
	}

private:
    ComPtr<ID3DBlob> m_blob = nullptr;
    eastl::wstring_view m_filePath = L"";
	eastl::string_view m_entryFunc = "";
	eastl::string_view m_target = "";
	std::vector<D3D_SHADER_MACRO> m_defines;
};


// @TODO: should we allow unloading shaders? does dx11 even allow unloading shaders?
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
    ShaderCompiler(std::unique_ptr<ID3DInclude>&& includer = nullptr) 
        : m_includer(std::move(includer))
    {}

    bool CompileShaderAsset(std::weak_ptr<ShaderAsset> asset);

private:
	std::unique_ptr<ID3DInclude> m_includer;

	struct CompiledResult {
		ComPtr<ID3DBlob> blob;
		ComPtr<ID3DBlob> error;
	};
	CompiledResult CompileShader(
		eastl::wstring_view filePath, 
		eastl::string_view entryFunc, 
		eastl::string_view target, 
		const std::vector<D3D_SHADER_MACRO>& defines);
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

