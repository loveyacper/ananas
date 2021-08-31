INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR})

ADD_EXECUTABLE(client_test TestClient.cc)
ADD_EXECUTABLE(server_test TestServer.cc)

SET(EXECUTABLE_OUTPUT_PATH  ${PROJECT_SOURCE_DIR}/bin/net_tests)

TARGET_LINK_LIBRARIES(client_test ananas_net)
TARGET_LINK_LIBRARIES(server_test ananas_net)

ADD_DEPENDENCIES(client_test ananas_net)
ADD_DEPENDENCIES(server_test ananas_net)

