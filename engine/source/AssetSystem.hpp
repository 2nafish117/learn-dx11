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


class AssetSystem {
public:
	AssetSystem() {
		spdlog::info("AssetSystem init");
		m_catalog = std::make_unique<AssetCatalog>("");
	}
	~AssetSystem() {
		spdlog::info("AssetSystem de-init");
	}

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


using AssetID = u32;


class AssetCatalog {
public:
	// @TODO: catalog file read
	AssetCatalog(std::string_view catalogPath, int initialCount = 128) {
		m_meshAssets.reserve(initialCount);
		// m_shaderAssets.reserve(initialCount);
		m_textureAssets.reserve(initialCount);
	}

	// @TODO: mesh asset is not shallow copyable
	AssetID RegisterMeshAsset(MeshAsset&& asset) { m_meshAssets.emplace_back(asset); }
	AssetID RegisterShaderAsset();
	AssetID RegisterTextureAsset();

private:
	std::vector<MeshAsset> m_meshAssets;
	// std::vector<ShaderAsset> m_shaderAssets;
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
		const std::vector<float3>& positions,
		const std::vector<float3>& normals, 
		const std::vector<float3>& tangents,
		const std::vector<float3>& colors,
		const std::vector<float2>& uv0s,
		const std::vector<float2>& uv1s,
		const std::vector<u32>& indices);

	virtual void Load() override;
	virtual void Unload() override;

	inline const std::vector<float3>& GetPositions() { return m_positions; }

	inline const std::vector<float3>& GetNormals() { return m_normals; }

	inline const std::vector<float3>& GetTangents() { return m_tangents; }

	inline const std::vector<float3>& GetColors() { return m_colors; }

	inline const std::vector<float2>& GetUV0s() { return m_uv0s; }

	inline const std::vector<float2>& GetUV1s() { return m_uv1s; }

	inline const std::vector<u32>& GetIndices() { return m_indices; }

private:
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
};

class TextureAsset : public Asset {
public:
	TextureAsset(std::string_view filePath);
	~TextureAsset();

	virtual void Load() override;
	virtual void Unload() override;

	inline int GetWidth() {
		return m_width;
	}
	
	inline int GetHeight() {
		return m_height;
	}

	inline int GetNumComponents() {
		return m_numComponents;
	}
	
	inline const byte* GetData() {
		return m_data;
	}
	
private:

	int m_width = 0;
	int m_height = 0;
	int m_numComponents = 0;

	stbi_uc* m_data = nullptr;

};