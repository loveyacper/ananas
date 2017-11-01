INCLUDE(${PROJECT_SOURCE_DIR}/CMakeCommon)

INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR})

ADD_EXECUTABLE(tryvoid_test TestTryVoid.cc)
ADD_EXECUTABLE(future_test TestFuture.cc)
ADD_EXECUTABLE(future_exception TestFutureException.cc)
ADD_EXECUTABLE(future_whenall_test TestFutureWhenAll.cc)
ADD_EXECUTABLE(future_whenall_iter_test TestFutureWhenAllIter.cc)
ADD_EXECUTABLE(future_whenany_test TestFutureWhenAny.cc)
ADD_EXECUTABLE(future_whenN_test TestFutureWhenN.cc)
ADD_EXECUTABLE(future_timeout TestFutureTimeout.cc)

TARGET_LINK_LIBRARIES(future_timeout ananas)
TARGET_LINK_LIBRARIES(future_test ananas)
TARGET_LINK_LIBRARIES(future_exception ananas)
ADD_DEPENDENCIES(future_timeout ananas)
ADD_DEPENDENCIES(future_test ananas)

SET(EXECUTABLE_OUTPUT_PATH  ${PROJECT_SOURCE_DIR}/bin/future_tests)

