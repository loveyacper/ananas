CMAKE_MINIMUM_REQUIRED(VERSION 2.6)

PROJECT(ANANAS)

SUBDIRS(net)
SUBDIRS(util)
find_package(Protobuf)

OPTION(USE_SSL "Use ssl or not" OFF)
IF(USE_SSL)
    ADD_DEFINITIONS(-DANANAS_DEBUG=1)
    SUBDIRS(ssl)
ENDIF()

OPTION(USE_PROTO "Use google protobuf or not" OFF)

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
