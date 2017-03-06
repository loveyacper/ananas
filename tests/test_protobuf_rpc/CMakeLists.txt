CMAKE_MINIMUM_REQUIRED(VERSION 2.8)

ADD_CUSTOM_COMMAND(OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/test_rpc.pb.h ${CMAKE_CURRENT_SOURCE_DIR}/test_rpc.pb.cc
    COMMAND protoc
    ARGS --cpp_out ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/test_rpc.proto -I${CMAKE_CURRENT_SOURCE_DIR} 
    DEPENDS test_rpc.proto)

INCLUDE(${PROJECT_SOURCE_DIR}/CMakeCommon) 
INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR})
INCLUDE_DIRECTORIES(/usr/local/Cellar/protobuf/3.2.0/include/)

# test servie
LINK_DIRECTORIES(/usr/local/Cellar/protobuf/3.2.0/lib)

ADD_EXECUTABLE(rpcclient test_rpc.pb.cc test_client.cc)
ADD_EXECUTABLE(rpcserver test_rpc.pb.cc test_server.cc)
SET(EXECUTABLE_OUTPUT_PATH  ${PROJECT_SOURCE_DIR}/bin/rpc_tests)

SET(ANANAS_RPC ananas_rpc)
TARGET_LINK_LIBRARIES(rpcclient ${ANANAS_RPC};ananas;libprotobuf.a)
ADD_DEPENDENCIES(rpcclient ${ANANAS_RPC};ananas;libprotobuf.a)
TARGET_LINK_LIBRARIES(rpcserver ${ANANAS_RPC};ananas;libprotobuf.a)
ADD_DEPENDENCIES(rpcserver ${ANANAS_RPC};ananas;libprotobuf.a)
