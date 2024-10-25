cmake_minimum_required(VERSION 3.15)

project(engine LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(TARGET_NAME
	engine
)

add_executable(${TARGET_NAME})
target_compile_options(${TARGET_NAME} PUBLIC -fno-exceptions)

add_subdirectory(src)

target_link_libraries(${TARGET_NAME}
	d3d11.lib
	dxgi.lib
	d3dcompiler.lib
	glfw
	spdlog
	stb
	tinygltf
	imgui
)

# @TODO: if assets change, i need a recopy
# @TODO: is it possible to just avoid this copy instead?? maybe pass a cmd line path to the data dir
add_custom_command(TARGET ${TARGET_NAME} 
	POST_BUILD 
	COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/${TARGET_NAME}/data $<TARGET_FILE_DIR:${TARGET_NAME}>/../data
	COMMENT "Copying shaders" VERBATIM
	COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/${TARGET_NAME}/data $<TARGET_FILE_DIR:${TARGET_NAME}>/data
	COMMENT "Copying shaders" VERBATIM
)