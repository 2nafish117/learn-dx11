cmake_minimum_required(VERSION 3.15)

project(engine LANGUAGES C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 17)

set(H_FILES
	src/Basic.hpp
	src/Application.hpp	
	src/Renderer.hpp
)

set(CPP_FILES
	src/Main.cpp
	
	src/Application.cpp
	src/Renderer.cpp
)

set(TARGET_NAME
	engine
)

add_executable(${TARGET_NAME} 
	${CPP_FILES}
	${H_FILES}
)

target_link_libraries(${TARGET_NAME}
	d3d11.lib
	dxgi.lib
	d3dcompiler.lib
	glfw
	gb
	spdlog
	tinygltf
)

# @TODO: changes to thesse do not trigger a recopy, make fake changes to cpp source to trigger
# add_custom_target(shaders
# 	SOURCES 
# 	data/shaders/color_ps.hlsl
# 	data/shaders/color_vs.hlsl
# 	data/shaders/texture_ps.hlsl
# 	data/shaders/texture_vs.hlsl
# 	data/shaders/light_ps.hlsl
# 	data/shaders/light_vs.hlsl
# )

# add_custom_command(TARGET ${TARGET_NAME} POST_BUILD 
# 	COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/${TARGET_NAME}/data $<TARGET_FILE_DIR:${TARGET_NAME}>/../data
# 	COMMENT "Copying shaders" VERBATIM
# 	COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/${TARGET_NAME}/data $<TARGET_FILE_DIR:${TARGET_NAME}>/data
# 	COMMENT "Copying shaders" VERBATIM
# )