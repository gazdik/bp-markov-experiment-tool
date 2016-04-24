set(CMAKE_INCLUDE_CURRENT_DIR ON)

file( GLOB ALL_SOURCES *.cc )

add_executable (clMarkovGen ${ALL_SOURCES})

if(WIN32)
  target_link_libraries(clMarkovGen ws2_32 ${OpenCL_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
else(WIN32)
  target_link_libraries(clMarkovGen ${OpenCL_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
endif(WIN32)

#target_link_libraries( clMarkovGen ${OpenCL_LIBRARIES} )
