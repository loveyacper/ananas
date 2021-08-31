INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR})

ADD_EXECUTABLE(coroutine_test TestCoroutine.cc)
SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin/tests)

TARGET_LINK_LIBRARIES(coroutine_test coroutine)
ADD_DEPENDENCIES(coroutine_test coroutine)
