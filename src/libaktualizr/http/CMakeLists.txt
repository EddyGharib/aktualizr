set(SOURCES httpclient.cc)

set(HEADERS ../../../include/libaktualizr/http/httpclient.h
            ../../../include/libaktualizr/http/httpinterface.h)

add_library(http OBJECT ${SOURCES})
target_link_libraries(http PUBLIC jsoncpp_lib)

add_aktualizr_test(NAME http_client SOURCES httpclient_test.cc PROJECT_WORKING_DIRECTORY)

aktualizr_source_file_checks(${SOURCES} ${HEADERS} ${TEST_SOURCES})
