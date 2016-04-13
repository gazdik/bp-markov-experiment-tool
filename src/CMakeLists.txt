set(CMAKE_INCLUDE_CURRENT_DIR ON)

file( GLOB ALL_SOURCES *.cc )

add_executable (clMarkovGen ${ALL_SOURCES})

if(WIN32)
  target_link_libraries(clMarkovGen ws2_32 OpenCL)
else(WIN32)
  target_link_libraries(clMarkovGen OpenCL)
endif(WIN32)

#target_link_libraries( clMarkovGen ${OPENCL_LIBRARIES} )

