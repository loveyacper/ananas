INCLUDE(${PROJECT_SOURCE_DIR}/CMakeCommon)

INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR})

ADD_EXECUTABLE(rediscli_coroutine_test RedisContext.cc;main.cc)

SET(EXECUTABLE_OUTPUT_PATH  ${PROJECT_SOURCE_DIR}/bin/examples)

TARGET_LINK_LIBRARIES(rediscli_coroutine_test ananas;coroutine)

ADD_DEPENDENCIES(rediscli_coroutine_test ananas;coroutine)

