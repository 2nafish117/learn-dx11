cmake_minimum_required(VERSION 3.15)

project(vendor
	DESCRIPTION "vendor libraries" 
)

# glfw stuff
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

add_subdirectory(glfw-3.4)

# gb stuff
add_subdirectory(gb)

# spdlog stuff
set(SPDLOG_WCHAR_SUPPORT ON)
set(SPDLOG_WCHAR_FILENAMES ON)
add_subdirectory(spdlog-1.14.1)

# tinygltf stuff
set(TINYGLTF_HEADER_ONLY ON CACHE INTERNAL "" FORCE)
set(TINYGLTF_INSTALL OFF CACHE INTERNAL "" FORCE)
add_subdirectory(tinygltf-2.9.3)

# stb stuff
add_subdirectory(stb)
