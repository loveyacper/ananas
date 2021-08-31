INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR})

ADD_EXECUTABLE(callunittest UnitTest.cc; CallUnitTests.cc)
ADD_EXECUTABLE(bufferunittest UnitTest.cc; BufferTest.cc)
ADD_EXECUTABLE(delegateunittest UnitTest.cc; DelegateTest.cc)

SET(EXECUTABLE_OUTPUT_PATH  ${PROJECT_SOURCE_DIR}/bin/unittests)

TARGET_LINK_LIBRARIES(bufferunittest ananas_util)
ADD_DEPENDENCIES(bufferunittest ananas_util)
