INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR})

INCLUDE(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG release-1.10.0
)
FetchContent_MakeAvailable(googletest)

SET(TEST_TARGET ananas_unittests)
ADD_EXECUTABLE(${TEST_TARGET} "")
SET(EXECUTABLE_OUTPUT_PATH  ${PROJECT_SOURCE_DIR}/bin/unittests)

TARGET_SOURCES(${TEST_TARGET}
  PRIVATE
  BufferTest.cc
  CallUnitTest.cc
  DelegateTest.cc
  ThreadPoolTest.cc
  # EventLoopTest.cc FIXME
  HttpParserTest.cc
  # HttpTest.cc FIXME
)

TARGET_LINK_LIBRARIES(${TEST_TARGET}
  PRIVATE
  ananas_net
  ananas_util
  gtest_main
)
ENABLE_TESTING()
ADD_TEST(
  NAME google_test
  COMMAND $<TARGET_FILE:${TEST_TARGET}>)
