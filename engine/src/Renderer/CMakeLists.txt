cmake_minimum_required(VERSION 3.15)

target_sources(${TARGET_NAME}
PRIVATE 
	DX11ContextUtils.hpp
	DX11Context.hpp
	DX11Context.cpp
	DX11ContextDebug.cpp
)