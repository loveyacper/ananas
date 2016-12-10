INCLUDE(${PROJECT_SOURCE_DIR}/CMakeCommon)

INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR})

ADD_EXECUTABLE(tryvoid_test TestTryVoid.cc)
ADD_EXECUTABLE(call_test TestCall.cc)
ADD_EXECUTABLE(future_test TestFuture.cc)
ADD_EXECUTABLE(future_whenall_test TestFutureWhenAll.cc)
ADD_EXECUTABLE(future_whenall_iter_test TestFutureWhenAllIter.cc)
ADD_EXECUTABLE(future_whenany_test TestFutureWhenAny.cc)

SET(EXECUTABLE_OUTPUT_PATH  ${PROJECT_SOURCE_DIR}/bin)

