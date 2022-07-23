INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR})

AUX_SOURCE_DIRECTORY(. ANANAS_SRC)
AUX_SOURCE_DIRECTORY(./http ANANAS_SRC)
SET(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)

IF(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    LIST(FILTER {ANANAS_SRC EXCLUDE REGEX ".*Kqueue.*")
ELSEIF(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    LIST(FILTER ANANAS_SRC EXCLUDE REGEX ".*Epoller.*")
ENDIF()


ADD_LIBRARY(ananas_net ${ANANAS_SRC})
target_link_libraries(
  ananas_net
  llhttp::llhttp
)
ADD_DEPENDENCIES(ananas_net ananas_util)
TARGET_LINK_LIBRARIES(ananas_net; ananas_util; pthread)
SET_TARGET_PROPERTIES(ananas_net PROPERTIES LINKER_LANGUAGE CXX)


INSTALL(TARGETS ananas_net DESTINATION lib)
set(HEADERS
    Application.h
    Connection.h
    EventLoop.h
    PipeChannel.h
    Poller.h
    Socket.h
    Typedefs.h
    http/HttpClient.h
    http/HttpServer.h
   )

INSTALL(FILES ${HEADERS} DESTINATION include/ananas/net)

