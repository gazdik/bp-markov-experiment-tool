cmake_minimum_required (VERSION 2.8)

project (experimentTool)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/")

if (CMAKE_VERSION VERSION_LESS 3.1)
	set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
endif (CMAKE_VERSION VERSION_LESS 3.1)

find_package(Threads REQUIRED)
find_package(OpenCL REQUIRED)

if (UNIX)
	add_definitions( -std=c++11 )
endif (UNIX)

SET(CMAKE_BUILD_TYPE Release)
#SET(CMAKE_BUILD_TYPE Debug)

if (WIN32)  
  include_directories(${CMAKE_SOURCE_DIR}/include/windows)  
endif (WIN32)

include_directories(${OpenCL_INCLUDE_DIRS})
add_subdirectory(src)

file(COPY src/CLMarkovPassGen.cl DESTINATION bin/kernels/)
file(COPY src/Cracker.cl DESTINATION bin/kernels/)
