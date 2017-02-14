CMAKE_MINIMUM_REQUIRED(VERSION 2.8)

EXECUTE_PROCESS(COMMAND protoc --cpp_out=./ test_rpc.proto  -I./
                WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/tests/test_protobuf_rpc)

INCLUDE(${PROJECT_SOURCE_DIR}/CMakeCommon) 
INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR})
INCLUDE_DIRECTORIES(/usr/local/Cellar/protobuf/3.2.0/include/)

# test servie
LINK_DIRECTORIES(/usr/local/Cellar/protobuf/3.2.0/lib)

ADD_EXECUTABLE(rpcclient test_rpc.pb.cc test_client.cc)
ADD_EXECUTABLE(rpcserver test_rpc.pb.cc test_server.cc)
SET(EXECUTABLE_OUTPUT_PATH  ${PROJECT_SOURCE_DIR}/bin)

SET(ANANAS_RPC ananas_rpc)
TARGET_LINK_LIBRARIES(rpcclient ${ANANAS_RPC};ananas;libprotobuf.a)
ADD_DEPENDENCIES(rpcclient ${ANANAS_RPC};ananas;libprotobuf.a)
TARGET_LINK_LIBRARIES(rpcserver ${ANANAS_RPC};ananas;libprotobuf.a)
ADD_DEPENDENCIES(rpcserver ${ANANAS_RPC};ananas;libprotobuf.a)
