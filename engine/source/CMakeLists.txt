cmake_minimum_required(VERSION 3.15)

target_precompile_headers(${TARGET_NAME}
PRIVATE
	Basic.hpp
)

target_sources(${TARGET_NAME}
PRIVATE 
	Basic.hpp

	Math.hpp

	Main.cpp
	
	Application.hpp	
	Application.cpp
	
	Camera.hpp
	Camera.cpp
	
	Shader.hpp
	Shader.cpp

	Importers.cpp
	Importers.hpp

	AssetSystem.cpp
	AssetSystem.hpp

	SceneSystem.cpp
	SceneSystem.hpp
)

add_subdirectory(DX11)

target_include_directories(${TARGET_NAME}
	PRIVATE .
)
