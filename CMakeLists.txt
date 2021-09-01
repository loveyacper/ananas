CMAKE_MINIMUM_REQUIRED(VERSION 3.14)

PROJECT(ANANAS)

SET(CMAKE_CXX_STANDARD 14)

IF(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    message("Detect linux platform")
ELSEIF(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    message("Detect mac platform")
ELSE()
    message(FATAL_ERROR "Only support linux or mac")
ENDIF()

OPTION(DEBUG "Debug or release" OFF)

IF(DEBUG)
    SET(CMAKE_BUILD_TYPE "Debug")
    message("${CMAKE_BUILD_TYPE}")
ELSE()
    SET(CMAKE_BUILD_TYPE "Release")
    message("${CMAKE_BUILD_TYPE}")
ENDIF()

INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/../)

SUBDIRS(net)
SUBDIRS(util)
SUBDIRS(future)

OPTION(USE_SSL "Use ssl or not" OFF)
IF(USE_SSL)
    ADD_DEFINITIONS(-DANANAS_DEBUG=1)
    SUBDIRS(ssl)
ENDIF()

OPTION(USE_PROTO "Use google protobuf or not" OFF)

FIND_PACKAGE(Protobuf)
IF(PROTOBUF_FOUND)
    MESSAGE(STATUS "found protobuf")
    SET(USE_PROTO ON)
    SUBDIRS(protobuf_rpc)
ENDIF()

IF(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    SUBDIRS(coroutine)
ENDIF()

SUBDIRS(tests)
SUBDIRS(unittest)
SUBDIRS(examples)
