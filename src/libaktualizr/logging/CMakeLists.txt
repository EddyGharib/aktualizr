set(SOURCES logging.cc logging_config.cc default_log_sink.cc)
set(HEADERS ../../../include/libaktualizr/logging/logging.h)

add_library(logging OBJECT ${SOURCES})
target_link_libraries(logging PUBLIC config)
aktualizr_source_file_checks(${SOURCES} ${HEADERS})
