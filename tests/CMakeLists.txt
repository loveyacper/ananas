SUBDIRS(test_net)
SUBDIRS(test_udp)
SUBDIRS(test_log)
SUBDIRS(test_timer)
SUBDIRS(test_future)

IF(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    SUBDIRS(test_coroutine)
ENDIF()

IF(USE_PROTO)
    SUBDIRS(test_protobuf_rpc)
ENDIF()
