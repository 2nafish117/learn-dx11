cmake_minimum_required(VERSION 3.15)

project(gb
	DESCRIPTION "gb headers" 
)

add_library(gb
	gb.cpp
)

target_include_directories(gb
	PUBLIC ${CMAKE_SOURCE_DIR}/vendor/gb/
)
