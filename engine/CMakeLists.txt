cmake_minimum_required(VERSION 3.15)

project(engine LANGUAGES C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 17)

set(H_FILES
	src/applicationclass.h
	src/d3dclass.h
	src/inputclass.h
	src/systemclass.h
	src/colorshaderclass.h
	src/modelclass.h
	src/cameraclass.h
)

set(CPP_FILES
	src/main.cpp
	src/applicationclass.cpp
	src/d3dclass.cpp
	src/inputclass.cpp
	src/systemclass.cpp
	src/colorshaderclass.cpp
	src/modelclass.cpp
	src/cameraclass.cpp
)

set(TARGET_NAME
	engine
)

add_executable(${TARGET_NAME} 
	${H_FILES}
	${CPP_FILES}
)

target_link_libraries(${TARGET_NAME}
	d3d11.lib
	dxgi.lib
	d3dcompiler.lib
)

# @TODO: changes to thesse do not trigger a recopy, make fake changes to cpp source to trigger
add_custom_target(shaders
	SOURCES 
	data/shaders/color_ps.hlsl
	data/shaders/color_vs.hlsl
)

add_custom_command(TARGET ${TARGET_NAME} POST_BUILD 
	COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/${TARGET_NAME}/data/shaders $<TARGET_FILE_DIR:${TARGET_NAME}>/../data/shaders
	COMMENT "Copying shaders" VERBATIM
)