CMAKE_MINIMUM_REQUIRED(VERSION 2.6)

SUBDIRS(test_net)
SUBDIRS(test_udp)
SUBDIRS(test_log)
SUBDIRS(test_timer)
SUBDIRS(test_future)

IF(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    SUBDIRS(test_coroutine)
ENDIF()

SUBDIRS(test_protobuf_rpc)
