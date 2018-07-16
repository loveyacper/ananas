CMAKE_MINIMUM_REQUIRED(VERSION 2.8)

set(HEADERS
    Future.h
    Try.h
    Helper.h
   )

INSTALL(FILES ${HEADERS} DESTINATION include/ananas/future)

