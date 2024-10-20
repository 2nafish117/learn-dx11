#include "Shader.hpp"

ShaderCompiler::ComPtr<ID3DBlob> ShaderCompiler::CompileShader(eastl::wstring_view filePath, eastl::string_view entryFunc, eastl::string_view target, D3D_SHADER_MACRO *defines) {
	ASSERT(filePath.data(), "");
	ASSERT(entryFunc.data(), "");

	ComPtr<ID3DBlob> bytecode;
	ComPtr<ID3DBlob> errors;

	UINT flags1 = D3DCOMPILE_WARNINGS_ARE_ERRORS | D3DCOMPILE_ENABLE_STRICTNESS;
	// something to do with fx files???
	UINT flags2 = 0;

	// @WTF
	// An optional array of D3D_SHADER_MACRO structures that define shader macros. Each macro definition contains a name and a null-terminated definition. 
	// If not used, set to NULL. The last structure in the array serves as a terminator and must have all members set to NULL

	if (auto res = D3DCompileFromFile(filePath.data(), defines, m_includer.get(), entryFunc.data(), target.data(), flags1, flags2, &bytecode, &errors); FAILED(res)) {
		DXERROR(res);
		spdlog::error("shader compile error: {}", (const char*)errors->GetBufferPointer());

		DEBUGBREAK();
	}

	return bytecode;
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

VertexShader::VertexShader(ComPtr<ID3D11Device> device, ComPtr<ID3DBlob> blob, eastl::string_view filePath) 
	: Base(blob, filePath), m_device(device)
{
	ASSERT(m_blob, "blob cant be null");

	// @TODO: what is this?
	ID3D11ClassLinkage* linkage = nullptr;
	m_device->CreateVertexShader(m_blob->GetBufferPointer(), m_blob->GetBufferSize(), linkage, &m_shader);
}

PixelShader::PixelShader(ComPtr<ID3D11Device> device, ComPtr<ID3DBlob> blob, eastl::string_view filePath) 
	: Base(blob, filePath), m_device(device)
{
	ASSERT(m_blob, "blob cant be null");

	// @TODO: what is this?
	ID3D11ClassLinkage* linkage = nullptr;
	m_device->CreatePixelShader(m_blob->GetBufferPointer(), m_blob->GetBufferSize(), linkage, &m_shader);
}