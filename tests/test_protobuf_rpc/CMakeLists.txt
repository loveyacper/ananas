CMAKE_MINIMUM_REQUIRED(VERSION 2.8)

ADD_CUSTOM_COMMAND(OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/test_rpc.pb.h ${CMAKE_CURRENT_SOURCE_DIR}/test_rpc.pb.cc
    COMMAND protoc
    ARGS --cpp_out ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/test_rpc.proto -I${CMAKE_CURRENT_SOURCE_DIR}
    DEPENDS test_rpc.proto)

INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR})

ADD_EXECUTABLE(server test_rpc.pb.cc server.cc)
ADD_EXECUTABLE(client test_rpc.pb.cc client.cc)
ADD_EXECUTABLE(blocking_client test_rpc.pb.cc client_blocking.cc)
SET(EXECUTABLE_OUTPUT_PATH  ${PROJECT_SOURCE_DIR}/bin/rpc_tests)

SET(ANANAS_RPC ananas_rpc)
TARGET_LINK_LIBRARIES(server ${ANANAS_RPC};ananas_net;libprotobuf.a)
ADD_DEPENDENCIES(server ${ANANAS_RPC};ananas_net;libprotobuf.a)
TARGET_LINK_LIBRARIES(client ${ANANAS_RPC};ananas_net;libprotobuf.a)
ADD_DEPENDENCIES(client ${ANANAS_RPC};ananas_net;libprotobuf.a)
TARGET_LINK_LIBRARIES(blocking_client ${ANANAS_RPC};ananas_net;libprotobuf.a)
ADD_DEPENDENCIES(blocking_client ${ANANAS_RPC};ananas_net;libprotobuf.a)
