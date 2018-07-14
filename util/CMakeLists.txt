CMAKE_MINIMUM_REQUIRED(VERSION 2.8)

INCLUDE(${PROJECT_SOURCE_DIR}/CMakeCommon)

INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR})

AUX_SOURCE_DIRECTORY(. UTIL_SRC)
SET(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)

ADD_LIBRARY(ananas_util ${UTIL_SRC})
SET_TARGET_PROPERTIES(ananas_util PROPERTIES LINKER_LANGUAGE CXX)

INSTALL(TARGETS ananas_util DESTINATION lib)
set(HEADERS
    Buffer.h
    Delegate.h
    ConfigParser.h
    Scheduler.h
    StringView.h
    ThreadPool.h
    Timer.h
    TimeUtil.h
    Util.h
    Logger.h
    MmapFile.h
   )
                      
INSTALL(FILES ${HEADERS} DESTINATION include/ananas/util)

