CMAKE_MINIMUM_REQUIRED(VERSION 2.8)

INCLUDE(${PROJECT_SOURCE_DIR}/CMakeCommon) 
INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR})

ADD_EXECUTABLE(ananas_nameserver server.cc)
SET(EXECUTABLE_OUTPUT_PATH  ${PROJECT_SOURCE_DIR}/bin/)

TARGET_LINK_LIBRARIES(ananas_nameserver ananas_rpc;ananas;libprotobuf.a)
ADD_DEPENDENCIES(ananas_nameserver ananas_rpc};ananas;libprotobuf.a)
