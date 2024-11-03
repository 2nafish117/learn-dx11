cmake_minimum_required(VERSION 3.15)

target_sources(${TARGET_NAME}
PRIVATE 
	DX11Context.hpp
	DX11Context.cpp
	DX11ContextDebug.cpp
	
	StaticMesh.hpp
	StaticMesh.cpp
	
	Texture.hpp
	Texture.cpp
)