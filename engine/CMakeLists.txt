cmake_minimum_required(VERSION 3.15)

project(engine LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(TARGET_NAME
	engine
)

add_executable(${TARGET_NAME})

if(MSVC)
	target_compile_options(${TARGET_NAME} 
	PRIVATE 
	/W4 
	# /WX
	)
else()
	target_compile_options(${TARGET_NAME} 
	PRIVATE 
	-Wall 
	-Wextra 
	-Wpedantic 
	# -Werror
	)
endif()

add_subdirectory(src)

target_link_libraries(${TARGET_NAME}
	d3d11.lib
	dxgi.lib
	d3dcompiler.lib
	glfw
	spdlog
	stb
	imgui
	cgltf
	flags
)

# Set the debugger arguments for Visual Studio
set_target_properties(${TARGET_NAME} PROPERTIES
    VS_DEBUGGER_COMMAND_ARGUMENTS ""
)

# @TODO: if assets change, i need a recopy
# @TODO: is it possible to just avoid this copy instead?? maybe pass a cmd line path to the data dir
add_custom_command(TARGET ${TARGET_NAME} 
	POST_BUILD 
	COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/${TARGET_NAME}/data $<TARGET_FILE_DIR:${TARGET_NAME}>/../data
	COMMENT "Copying data" VERBATIM
	COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/${TARGET_NAME}/data $<TARGET_FILE_DIR:${TARGET_NAME}>/data
	COMMENT "Copying data" VERBATIM
)