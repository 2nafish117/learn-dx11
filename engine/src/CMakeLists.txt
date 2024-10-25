cmake_minimum_required(VERSION 3.15)

target_sources(${TARGET_NAME}
PRIVATE 
	Basic.hpp
	Math.hpp
	Application.hpp	
	Scene.hpp
	Mesh.hpp
	Camera.hpp
	Shader.hpp

	Main.cpp
	Application.cpp
	Scene.cpp
	Mesh.cpp
	Camera.cpp
	Shader.cpp
)

add_subdirectory(Renderer)

target_include_directories(${TARGET_NAME}
	PRIVATE .
)
