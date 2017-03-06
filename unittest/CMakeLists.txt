INCLUDE(${PROJECT_SOURCE_DIR}/CMakeCommon)
INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR})

#AUX_SOURCE_DIRECTORY(. UNITTEST_SRC)

ADD_EXECUTABLE(callunittest UnitTest.cc; CallUnitTests.cc)
ADD_EXECUTABLE(bufferunittest UnitTest.cc; BufferTest.cc)
ADD_EXECUTABLE(delegateunittest UnitTest.cc; DelegateTest.cc)

SET(EXECUTABLE_OUTPUT_PATH  ${PROJECT_SOURCE_DIR}/bin/unittests)

TARGET_LINK_LIBRARIES(bufferunittest ananas)
ADD_DEPENDENCIES(bufferunittest ananas)
