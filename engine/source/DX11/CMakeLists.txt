cmake_minimum_required(VERSION 3.15)

target_sources(${TARGET_NAME}
PRIVATE 
	DX11ContextUtils.hpp
	DX11Context.hpp
	DX11Context.cpp
	DX11ContextDebug.cpp
	
	DX11Mesh.hpp
	DX11Mesh.cpp
	
	DX11Shader.hpp
	DX11Shader.cpp

	DX11Texture.hpp
	DX11Texture.cpp
)