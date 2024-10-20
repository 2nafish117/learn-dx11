#include "Shader.hpp"

bool ShaderCompiler::CompileShaderAsset(std::weak_ptr<ShaderAsset> asset)
{
	if(auto a = asset.lock(); a != nullptr) {
		CompiledResult res = CompileShader(
			a->GetFilePath(),
			a->GetEntryFunc(),
			a->GetTarget(),
			a->GetDefines()
		);

		a->SetBlob(res.blob);

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
	eastl::wstring_view filePath, 
	eastl::string_view entryFunc, 
	eastl::string_view target, 
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

	if (auto res = D3DCompileFromFile(filePath.data(), exDefines, m_includer.get(), entryFunc.data(), target.data(), flags1, flags2, &compiled.blob, &compiled.error); FAILED(res)) {
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
		auto blob = sa->GetBlob();
		ASSERT(blob, "");

		// @TODO: what is this?
		ID3D11ClassLinkage* linkage = nullptr;
		
		if(auto res = m_device->CreateVertexShader(blob->GetBufferPointer(), blob->GetBufferSize(), linkage, &m_shader); FAILED(res)) {
			DXERROR(res);
		}
	}
}

PixelShader::PixelShader(ComPtr<ID3D11Device> device, std::weak_ptr<ShaderAsset> asset) 
	: Base(device, asset)
{
	if(auto sa = m_shaderAsset.lock(); sa != nullptr) {
		auto blob = sa->GetBlob();
		ASSERT(blob, "");
		
		// @TODO: what is this?
		ID3D11ClassLinkage* linkage = nullptr;
		
		if(auto res = m_device->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), linkage, &m_shader); FAILED(res)) {
			DXERROR(res);
		}
	}
}
