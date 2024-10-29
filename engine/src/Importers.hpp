#pragma once

#include <Basic.hpp>

// @TODO: move this to its own file
class AssetSystem {
public:
	// eh...
	inline std::string DataDir() {
		return m_dataDir.generic_string();
	}

	// converts an assets virtual path (relative to dataDir) to a real path on disk
	// @TODO: this allocates memory, make it not do that
	// this is temporary
	inline std::string GetRealPath(std::string_view path)
	{
		// operator overloading, ughh...
		std::filesystem::path realPath = m_dataDir / path;
		return realPath.generic_string();
	}

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

	friend class Application;
};

namespace global {
	extern std::unique_ptr<AssetSystem> assetSystem;
}

// @TODO
class GltfImporter {
public:
private:
};