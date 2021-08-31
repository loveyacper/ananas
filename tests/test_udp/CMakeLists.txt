INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR})

ADD_EXECUTABLE(client_udptest TestClient.cc)
ADD_EXECUTABLE(server_udptest TestServer.cc)

SET(EXECUTABLE_OUTPUT_PATH  ${PROJECT_SOURCE_DIR}/bin/net_tests)

TARGET_LINK_LIBRARIES(client_udptest ananas_net)
TARGET_LINK_LIBRARIES(server_udptest ananas_net)

ADD_DEPENDENCIES(client_udptest ananas_net)
ADD_DEPENDENCIES(server_udptest ananas_net)

