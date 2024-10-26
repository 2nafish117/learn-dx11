cmake_minimum_required(VERSION 3.15)

target_sources(${TARGET_NAME}
PRIVATE 
	Basic.hpp
	Math.hpp

	Main.cpp
	
	Application.hpp	
	Application.cpp
	
	Scene.hpp
	Scene.cpp
	
	Mesh.hpp
	Mesh.cpp
	
	Camera.hpp
	Camera.cpp
	
	Shader.hpp
	Shader.cpp

	Importers.cpp
	Importers.hpp
)

add_subdirectory(Renderer)

target_include_directories(${TARGET_NAME}
	PRIVATE .
)
