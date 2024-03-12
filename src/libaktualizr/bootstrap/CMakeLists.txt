set(HEADERS bootstrap.h)
set(SOURCES bootstrap.cc)

add_library(bootstrap OBJECT ${SOURCES})
target_link_libraries(bootstrap PUBLIC crypto)
aktualizr_source_file_checks(${SOURCES} ${HEADERS})
