INCLUDE_DIRECTORIES(${Protobuf_INCLUDE_DIRS})
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS test_rpc.proto)

INCLUDE_DIRECTORIES(${PROJECT_BINARY_DIR})
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_BINARY_DIR})
INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR})

ADD_EXECUTABLE(server test_rpc.pb.cc server.cc)
ADD_EXECUTABLE(client test_rpc.pb.cc client.cc)
ADD_EXECUTABLE(blocking_client test_rpc.pb.cc client_blocking.cc)
SET(EXECUTABLE_OUTPUT_PATH  ${PROJECT_SOURCE_DIR}/bin/rpc_tests)

SET(ANANAS_RPC ananas_rpc)
TARGET_LINK_LIBRARIES(server ${ANANAS_RPC};ananas_net;${Protobuf_LIBRARY})
ADD_DEPENDENCIES(server ${ANANAS_RPC};ananas_net)
TARGET_LINK_LIBRARIES(client ${ANANAS_RPC};ananas_net;${Protobuf_LIBRARY})
ADD_DEPENDENCIES(client ${ANANAS_RPC};ananas_net)
TARGET_LINK_LIBRARIES(blocking_client ${ANANAS_RPC};ananas_net;${Protobuf_LIBRARY})
ADD_DEPENDENCIES(blocking_client ${ANANAS_RPC};ananas_net)
