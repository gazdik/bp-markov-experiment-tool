cmake_minimum_required (VERSION 2.8)

project (clMarkovPassGen)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/")
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

#set(CMAKE_CXX_FLAGS "-pthread")

add_definitions( -std=c++11 )
add_definitions( -DNDEBUG )

SET(CMAKE_BUILD_TYPE Release)
#SET(CMAKE_BUILD_TYPE Debug)

add_subdirectory(src)

file(COPY src/CLMarkovPassGen.cl DESTINATION bin/kernels/)
