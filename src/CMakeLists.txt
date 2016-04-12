set(CMAKE_INCLUDE_CURRENT_DIR ON)

file( GLOB ALL_SOURCES *.cc )

add_executable (clMarkovGen ${ALL_SOURCES})

target_link_libraries(clMarkovGen OpenCL)
if(WIN32)
  target_link_libraries(cmMarkovGen ws2_32)
endif()

#target_link_libraries( clMarkovGen ${OPENCL_LIBRARIES} )

