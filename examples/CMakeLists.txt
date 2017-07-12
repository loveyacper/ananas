CMAKE_MINIMUM_REQUIRED(VERSION 2.6)

SUBDIRS(redis_client_future_lite/)
SUBDIRS(redis_server_lite/)

IF(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    SUBDIRS(redis_client_coroutine_lite/)
ENDIF()

IF(USE_SSL)
SUBDIRS(ssl_test/)

ENDIF()
