#pragma once

#include "Basic.hpp"
#include "Math.hpp"

#include <stb/stb_image.h>

class AssetSystem;
namespace global 
{
	extern AssetSystem* assetSystem;
}

enum class AssetState;

class AssetCatalog;
class Asset;
class MeshAsset;
class ShaderAsset;
class TextureAsset;

class DX11Mesh;
class DX11Texture;
class DX11ShaderBase;

#define DECL_ASSET_ID(name, internalType) struct name { internalType value; }

DECL_ASSET_ID(AssetID, u32);

class AssetSystem {
public:
	AssetSystem() {
		spdlog::info("AssetSystem init");
		m_catalog = std::make_unique<AssetCatalog>("");
	}
	~AssetSystem() {
		spdlog::info("AssetSystem de-init");
	}

	void RegisterAssets();

	inline const AssetCatalog* Catalog() { return m_catalog.get(); }

	// eh...
	inline std::string DataDir() {
		return m_dataDir.generic_string();
	}

	// converts an assets virtual path (relative to dataDir) to a real path on disk
	// @TODO: this allocates memory, make it not do that, use allocator
	// this is temporary
	inline std::string GetRealPath(std::string_view path)
	{
		// operator overloading, ughh...
		std::filesystem::path realPath = m_dataDir / path;
		return realPath.generic_string();
	}

	// converts an assets virtual path (relative to dataDir) to a real path on disk
	// @TODO: this allocates memory, make it not do that
	// this is temporary
	inline std::wstring GetRealPath(std::wstring_view path)
	{
		// operator overloading, ughh...
		std::filesystem::path realPath = m_dataDir / path;
		return realPath.generic_wstring();
	}

private:
	inline void SetDataDir(std::string_view dir) {
		m_dataDir = dir;
	}

private:
	std::filesystem::path m_dataDir = "data";
	std::unique_ptr<AssetCatalog> m_catalog = nullptr;

	// only allow Application to set the data directory
	friend class Application;
};

DECL_ASSET_ID(MeshID, u32);
DECL_ASSET_ID(ShaderID, u32);
DECL_ASSET_ID(TextureID, u32);

class AssetCatalog {
public:
	// @TODO: catalog file read
	AssetCatalog(std::string_view catalogPath, int initialCount = 128) {
		m_meshAssets.reserve(initialCount);
		m_shaderAssets.reserve(initialCount);
		m_textureAssets.reserve(initialCount);
	}

	// @TODO: mesh asset is not shallow copyable, make them shallow copyable
	MeshID RegisterMeshAsset(MeshAsset&& asset) 
	{
		m_meshAssets.emplace_back(asset);
		MeshID id = { m_meshAssets.size() - 1 };
		return id;
	}
	ShaderID RegisterShaderAsset(ShaderAsset&& asset)
	{
		m_shaderAssets.emplace_back(asset);
		ShaderID id = { m_shaderAssets.size() - 1 };
		return id;
	}
	TextureID RegisterTextureAsset(TextureAsset&& asset)
	{
		m_textureAssets.emplace_back(asset);
		TextureID id = { m_textureAssets.size() - 1 };
		return id;
	}

	const MeshAsset& GetMeshAsset(MeshID id) const 
	{
		ENSURE(id.value >= 0 && id.value < m_meshAssets.size(), "");
		return m_meshAssets[id.value];
	}
	const ShaderAsset& GetShaderAsset(ShaderID id) const 
	{
		ENSURE(id.value >= 0 && id.value < m_shaderAssets.size(), "");
		return m_shaderAssets[id.value];
	}
	const TextureAsset& GetTextureAsset(TextureID id) const 
	{
		ENSURE(id.value >= 0 && id.value < m_textureAssets.size(), "");
		return m_textureAssets[id.value];
	}

private:
	std::vector<MeshAsset> m_meshAssets;
	std::vector<ShaderAsset> m_shaderAssets;
	std::vector<TextureAsset> m_textureAssets;
};


enum class AssetState {
	Unloaded,
	Loaded,
	Loading,
	Unloading,

	Num
};


// @TODO: this is only supposed to be the metadata of the asset
// real asset storage goes elsewhere?
class Asset {
public:
	AssetState state = AssetState::Unloaded;

public:
	virtual void Load() = 0;
	virtual void Unload() = 0;

protected:
	Asset() = default;
};

struct cgltf_data;

class MeshAsset : public Asset {
public:
	MeshAsset(std::string_view filePath);
	MeshAsset(
		const std::vector<float3>&& positions,
		const std::vector<float3>&& normals, 
		const std::vector<float3>&& tangents,
		const std::vector<float3>&& colors,
		const std::vector<float2>&& uv0s,
		const std::vector<float2>&& uv1s,
		const std::vector<u32>&& indices);

	virtual void Load() override;
	virtual void Unload() override;
	void* GetRendererResource() const;
	void InitRendererResource();

	inline const std::vector<float3>& GetPositions() { return m_positions; }
	inline const std::vector<float3>& GetNormals() { return m_normals; }
	inline const std::vector<float3>& GetTangents() { return m_tangents; }
	inline const std::vector<float3>& GetColors() { return m_colors; }
	inline const std::vector<float2>& GetUV0s() { return m_uv0s; }
	inline const std::vector<float2>& GetUV1s() { return m_uv1s; }
	inline const std::vector<u32>& GetIndices() { return m_indices; }

private:
	// @TODO: move this stuff to gltf importer
	void GltfPrintInfo(cgltf_data* data);

	void GltfPrintMeshInfo(cgltf_data* data);
	void GltfPrintAnimationInfo(cgltf_data* data);
	void GltfPrintMaterialInfo(cgltf_data* data);
	void GltfPrintImageInfo(cgltf_data* data);

private:
	std::string_view m_filePath;
	// @TODO: store vertices in SOA
	//std::vector<Vertex> m_vertices;
	
	std::vector<u32> m_indices;

	std::vector<float3> m_positions;
	std::vector<float3> m_normals;
	std::vector<float3> m_tangents;
	std::vector<float3> m_colors;
	std::vector<float2> m_uv0s;
	std::vector<float2> m_uv1s;

	DX11Mesh* m_rendererResource = nullptr;
};

class TextureAsset : public Asset {
public:
	TextureAsset(std::string_view filePath);
	~TextureAsset();

	virtual void Load() override;
	virtual void Unload() override;
	void* GetRendererResource() const;
	void InitRendererResource();

	inline int GetWidth() const { return m_width; }
	inline int GetHeight() const { return m_height; }
	inline int GetNumComponents() const { return m_numComponents; }
	inline const byte* GetData() const { return m_data; }
	
private:
	std::string_view m_filePath;
	
	int m_width = 0;
	int m_height = 0;
	int m_numComponents = 0;

	stbi_uc* m_data = nullptr;

	DX11Texture* m_rendererResource = nullptr;
};


struct ShaderMacro
{
	const char* name;
	const char* definition;
};

class ShaderAsset : public Asset {

public:
	enum class Kind {
		Invalid = 0,
		Vertex = 1,
		Pixel,

		Num
	};

	ShaderAsset(Kind kind, std::wstring_view filePath, std::string_view entryFunc, std::string_view target, const std::vector<ShaderMacro>& defines = {})
		: m_kind(kind), m_filePath(filePath), m_entryFunc(entryFunc), m_target(target), m_defines(defines) {}

	virtual void Load() override;
	virtual void Unload() override;
	void* GetRendererResource() const;
	void InitRendererResource();

	inline std::wstring_view GetFilePath() const { return m_filePath; }
	inline std::string_view GetEntryFunc() const { return m_entryFunc; }
	inline std::string_view GetTarget() const { return m_target; }
	inline const std::vector<ShaderMacro>& GetDefines() const { return m_defines; }

public:
	const byte* blob = nullptr;
	size_t blobSize = 0;

private:
	std::wstring_view m_filePath = L"";
	std::string_view m_entryFunc = "";
	std::string_view m_target = "";
	std::vector<ShaderMacro> m_defines;
	Kind m_kind = Kind::Invalid;

	DX11ShaderBase* m_rendererResource = nullptr;
};
