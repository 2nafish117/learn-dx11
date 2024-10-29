cmake_minimum_required(VERSION 3.15)

project(learn-dx11 
	DESCRIPTION "simple dx11 renderer" 
	LANGUAGES C CXX
)

add_subdirectory(vendor)

add_subdirectory(rastertek)

add_subdirectory(engine)

# Set engine as the startup project in Visual Studio
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT engine)