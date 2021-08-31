IF(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    INCLUDE_DIRECTORIES(/usr/local/Cellar/openssl/1.0.2l/include/)
    LINK_DIRECTORIES(/usr/local/Cellar/openssl/1.0.2l/lib/)
ENDIF()

INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR})


ADD_EXECUTABLE(ssl_server_test ssl_server.cc)
ADD_EXECUTABLE(ssl_client_test ssl_client.cc)

file(GLOB CERT_FILES RELATIVE ${PROJECT_SOURCE_DIR}/examples/ssl_test/
     "*.pem")
file(COPY ${PROJECT_SOURCE_DIR}/examples/ssl_test/${CERT_FILES} DESTINATION ${PROJECT_SOURCE_DIR}/bin/examples/)

SET(EXECUTABLE_OUTPUT_PATH  ${PROJECT_SOURCE_DIR}/bin/examples)

TARGET_LINK_LIBRARIES(ssl_server_test ananas_net; ananas_ssl)
TARGET_LINK_LIBRARIES(ssl_client_test ananas_net; ananas_ssl)

ADD_DEPENDENCIES(ssl_server_test ananas_net; ananas_ssl)
ADD_DEPENDENCIES(ssl_client_test ananas_net; ananas_ssl)

