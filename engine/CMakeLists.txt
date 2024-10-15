cmake_minimum_required(VERSION 3.15)

project(engine LANGUAGES C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(H_FILES
	src/Basic.hpp
	src/Math.hpp
	src/Application.hpp	

	src/Renderer.hpp
	src/RendererUtils.hpp
	src/Mesh.hpp
	src/Camera.hpp
	src/Shader.hpp
)

set(CPP_FILES
	src/Main.cpp
	
	src/Application.cpp

	src/Renderer.cpp
	src/Mesh.cpp
	src/Camera.cpp
	src/Shader.cpp
)

set(TARGET_NAME
	engine
)

add_executable(${TARGET_NAME} 
	${CPP_FILES}
	${H_FILES}
)

target_compile_options(${TARGET_NAME} PUBLIC -fno-exceptions)

target_link_libraries(${TARGET_NAME}
	d3d11.lib
	dxgi.lib
	d3dcompiler.lib
	glfw
	gb
	spdlog
	stb
	tinygltf
	imgui
	EASTL
)

add_custom_command(TARGET ${TARGET_NAME} 
	POST_BUILD 
	COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/${TARGET_NAME}/data $<TARGET_FILE_DIR:${TARGET_NAME}>/../data
	COMMENT "Copying shaders" VERBATIM
	COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/${TARGET_NAME}/data $<TARGET_FILE_DIR:${TARGET_NAME}>/data
	COMMENT "Copying shaders" VERBATIM
)