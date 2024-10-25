cmake_minimum_required(VERSION 3.15)

target_sources(${TARGET_NAME}
PRIVATE 
	DX11ContextUtils.hpp
	DX11Context.hpp
	DX11Context.cpp
	DX11ContextDebug.cpp
)

# target_include_directories(${TARGET_NAME} PUBLIC ${CMAKE_CURRENT_LIST_DIR})