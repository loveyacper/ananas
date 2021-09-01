INCLUDE_DIRECTORIES(${Protobuf_INCLUDE_DIRS})
INCLUDE_DIRECTORIES(${PROJECT_BINARY_DIR})
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ananas_rpc.proto)

INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR})
INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/protobuf_rpc/name_service_protocol)

AUX_SOURCE_DIRECTORY(. ANANAS_SRC)
AUX_SOURCE_DIRECTORY(./name_service_protocol ANANAS_SRC)
AUX_SOURCE_DIRECTORY(./codec ANANAS_SRC)

SET(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
SET(ANANAS_RPC ananas_rpc)

ADD_LIBRARY(${ANANAS_RPC} ${ANANAS_SRC};ananas_rpc.pb.cc)
TARGET_LINK_LIBRARIES(${ANANAS_RPC}; ananas_net; ${Protobuf_LIBRARY})
SET_TARGET_PROPERTIES(${ANANAS_RPC} PROPERTIES LINKER_LANGUAGE CXX)

INSTALL(TARGETS ananas_rpc DESTINATION lib)
set(HEADERS
    RpcServer.h
    RpcService.h
    RpcServiceStub.h
    RpcEndpoint.h
    RpcException.h
    ProtobufCoder.h
    ananas_rpc.pb.h
    name_service_protocol/RedisClientContext.h
   )

INSTALL(FILES ${HEADERS} DESTINATION include/ananas/rpc)


