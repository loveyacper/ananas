CMAKE_MINIMUM_REQUIRED(VERSION 2.8)

ADD_CUSTOM_COMMAND(OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/ananas_rpc.pb.h ${CMAKE_CURRENT_SOURCE_DIR}/ananas_rpc.pb.cc
    COMMAND protoc
    ARGS --cpp_out ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/ananas_rpc.proto -I${CMAKE_CURRENT_SOURCE_DIR}
    DEPENDS ananas_rpc.proto)

INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR})
INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/protobuf_rpc/name_service_protocol)

AUX_SOURCE_DIRECTORY(. ANANAS_SRC)
AUX_SOURCE_DIRECTORY(./name_service_protocol ANANAS_SRC)
AUX_SOURCE_DIRECTORY(./codec ANANAS_SRC)

SET(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
SET(ANANAS_RPC ananas_rpc)

ADD_LIBRARY(${ANANAS_RPC} ${ANANAS_SRC};ananas_rpc.pb.cc)
TARGET_LINK_LIBRARIES(${ANANAS_RPC}; ananas_net)
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


