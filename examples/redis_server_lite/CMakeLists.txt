INCLUDE(${PROJECT_SOURCE_DIR}/CMakeCommon)

INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR})

ADD_EXECUTABLE(redis_server_lite RedisContext.cc;Protocol.cc;main.cc)

SET(EXECUTABLE_OUTPUT_PATH  ${PROJECT_SOURCE_DIR}/bin/examples)

TARGET_LINK_LIBRARIES(redis_server_lite ananas)

ADD_DEPENDENCIES(redis_server_lite ananas)

