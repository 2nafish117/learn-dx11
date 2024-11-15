#include "DX11Shader.hpp"

#include "AssetSystem.hpp"

#include <d3dcompiler.h>
#include <d3dcommon.h>

bool ShaderCompiler::CompileShaderAsset(ShaderID asset)
{
    ShaderAsset& shaderAsset = const_cast<ShaderAsset&>(global::assetSystem->Catalog()->GetShaderAsset(asset));

    // @TODO: meh, copy
    const std::vector<ShaderMacro>& defines = shaderAsset.GetDefines();
    std::vector<D3D_SHADER_MACRO> shaderMacros;
    
    for(const auto& define: defines) {
        shaderMacros.push_back(D3D_SHADER_MACRO{define.name, define.definition});
    }

	std::wstring_view filePath = shaderAsset.GetFilePath();
	std::string_view entryFunc = shaderAsset.GetEntryFunc();
	std::string_view target = shaderAsset.GetTarget();

	//spdlog::info("compiling shader {}", filePath.data());

	CompiledResult res = CompileShader(
			filePath,
			entryFunc,
			target,
			shaderMacros
		);

	if (res.error != nullptr) {
		const char* errStr = (const char*)res.error->GetBufferPointer();
		spdlog::error("shader compile error: {}", errStr);
		DEBUGBREAK();
		return false;
	}

	shaderAsset.blobSize = res.blob->GetBufferSize();

    // @TODO: use temp allocator here
	shaderAsset.blob = (const byte*) malloc(shaderAsset.blobSize);
	memcpy_s((void*)shaderAsset.blob, shaderAsset.blobSize, res.blob->GetBufferPointer(), shaderAsset.blobSize);

	return true;
}

ShaderCompiler::CompiledResult ShaderCompiler::CompileShader(
	std::wstring_view filePath, 
	std::string_view entryFunc, 
	std::string_view target, 
	const std::vector<D3D_SHADER_MACRO>& defines) {

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

DX11VertexShader::DX11VertexShader(ComPtr<ID3D11Device> device, const CreateInfo& info)
	: Base(device)
{
	Create(device, info);
}

void DX11VertexShader::Create(ComPtr<ID3D11Device> device, const CreateInfo& info)
{
	ASSERT(info.blob != nullptr, "");

	// @TODO: what is this?
	ID3D11ClassLinkage* linkage = nullptr;

	if (auto res = device->CreateVertexShader(info.blob, info.blobSize, linkage, &m_shader); FAILED(res)) {
		DXERROR(res);
	}
}

DX11PixelShader::DX11PixelShader(ComPtr<ID3D11Device> device, const CreateInfo& info)
	: Base(device)
{
	Create(device, info);
}

void DX11PixelShader::Create(ComPtr<ID3D11Device> device, const CreateInfo& info)
{

	ASSERT(info.blob != nullptr, "");
	// @TODO: what is this?
	ID3D11ClassLinkage* linkage = nullptr;

	if (auto res = device->CreatePixelShader(info.blob, info.blobSize, linkage, &m_shader); FAILED(res)) {
		DXERROR(res);
	}
}
