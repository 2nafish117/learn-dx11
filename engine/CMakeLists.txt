cmake_minimum_required(VERSION 3.15)

project(engine LANGUAGES C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 17)

set(H_FILES
	src/applicationclass.h
	src/d3dclass.h
	src/inputclass.h
	src/systemclass.h
)

set(CPP_FILES
	src/main.cpp
	src/applicationclass.cpp
	src/d3dclass.cpp
	src/inputclass.cpp
	src/systemclass.cpp
)

# set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS /ENTRY:wWinMain")

add_executable(engine 
	${H_FILES}
	${CPP_FILES}
)

target_link_libraries(engine
	d3d11.lib
	dxgi.lib
	d3dcompiler.lib
)