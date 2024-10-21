cmake_minimum_required(VERSION 3.15)

project(engine LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(TARGET_NAME
	engine
)

set(H_FILES
	DX11Context.hpp
	DX11ContextUtils.hpp
)

set(CPP_FILES
	DX11Context.cpp
	DX11ContextDebug.cpp
)

# target_include_directories(

# )

