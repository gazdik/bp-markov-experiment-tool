set(CMAKE_INCLUDE_CURRENT_DIR ON)

file( GLOB ALL_SOURCES *.cc )

add_executable (clMarkovGen ${ALL_SOURCES})

#target_link_libraries(clMarkovGen OpenCL)
target_link_libraries( clMarkovGen ${OPENCL_LIBRARIES} )

