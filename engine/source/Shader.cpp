#include "Shader.hpp"

#include "AssetSystem.hpp"

#include <d3dcompiler.h>
#include <d3dcommon.h>

bool ShaderCompiler::CompileShaderAsset(std::weak_ptr<ShaderAsset> asset)
{
	if(auto a = asset.lock(); a != nullptr) {
		CompiledResult res = CompileShader(
			a->GetFilePath(),
			a->GetEntryFunc(),
			a->GetTarget(),
			a->GetDefines()
		);

		a->blobSize = res.blob->GetBufferSize();

		a->blob = (const byte*) malloc(a->blobSize);
		memcpy_s((void*)a->blob, a->blobSize, res.blob->GetBufferPointer(), a->blobSize);

		if(res.error != nullptr) {
			const char* errStr = (const char*)res.error->GetBufferPointer();
			spdlog::error("shader compile error: {}", errStr);
			DEBUGBREAK();
			return false;
		}
	}

	return true;
}

ShaderCompiler::CompiledResult ShaderCompiler::CompileShader(
	std::wstring_view filePath, 
	std::string_view entryFunc, 
	std::string_view target, 
	const std::vector<ShaderMacro>& defines) {

	ASSERT(filePath.data(), "");
	ASSERT(entryFunc.data(), "");
	
	CompiledResult compiled;

	UINT flags1 = D3DCOMPILE_WARNINGS_ARE_ERRORS | D3DCOMPILE_ENABLE_STRICTNESS;
	// something to do with fx files???
	UINT flags2 = 0;

	// @WTF
	// An optional array of D3D_SHADER_MACRO structures that define shader macros. Each macro definition contains a name and a null-terminated definition. 
	// If not used, set to NULL. The last structure in the array serves as a terminator and must have all members set to NULL

	// @TODO: extract defines as needed
	D3D_SHADER_MACRO* exDefines = nullptr;

	std::wstring realPath = global::assetSystem->GetRealPath(filePath);
	if (auto res = D3DCompileFromFile(realPath.data(), exDefines, m_includer, entryFunc.data(), target.data(), flags1, flags2, &compiled.blob, &compiled.error); FAILED(res)) {
		DXERROR(res);
	}

	return compiled;
}

HRESULT ShaderIncluder::Open(D3D_INCLUDE_TYPE IncludeType, LPCSTR pFileName, LPCVOID pParentData, LPCVOID *ppData, UINT *pBytes) {
	
	static const char* includeTypeStrings[] = {
		"D3D_INCLUDE_LOCAL",
        "D3D_INCLUDE_SYSTEM",
        "D3D10_INCLUDE_LOCAL",
        "D3D10_INCLUDE_SYSTEM",
        "D3D_INCLUDE_FORCE_DWORD",
	};

	spdlog::error("shader includes not yet supported, implement ShaderIncluder fully");

	return S_OK;
}

HRESULT ShaderIncluder::Close(LPCVOID pData) {
	return S_OK;
}

VertexShader::VertexShader(ComPtr<ID3D11Device> device, std::weak_ptr<ShaderAsset> asset) 
	: Base(device, asset)
{
	if(auto sa = m_shaderAsset.lock(); sa != nullptr) {
		ASSERT(sa->blob, "");

		// @TODO: what is this?
		ID3D11ClassLinkage* linkage = nullptr;
		
		if(auto res = m_device->CreateVertexShader(sa->blob, sa->blobSize, linkage, &m_shader); FAILED(res)) {
			DXERROR(res);
		}
	}
}

PixelShader::PixelShader(ComPtr<ID3D11Device> device, std::weak_ptr<ShaderAsset> asset) 
	: Base(device, asset)
{
	if(auto sa = m_shaderAsset.lock(); sa != nullptr) {
		ASSERT(sa->blob, "");
		
		// @TODO: what is this?
		ID3D11ClassLinkage* linkage = nullptr;
		
		if(auto res = m_device->CreatePixelShader(sa->blob, sa->blobSize, linkage, &m_shader); FAILED(res)) {
			DXERROR(res);
		}
	}
}
