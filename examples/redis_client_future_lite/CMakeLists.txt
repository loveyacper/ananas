INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR})

ADD_EXECUTABLE(rediscli_future_test RedisContext.cc;main.cc)

SET(EXECUTABLE_OUTPUT_PATH  ${PROJECT_SOURCE_DIR}/bin/examples)

TARGET_LINK_LIBRARIES(rediscli_future_test ananas_net)

ADD_DEPENDENCIES(rediscli_future_test ananas_net)

