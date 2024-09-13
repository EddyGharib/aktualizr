set(SOURCES managedsecondary.cc virtualsecondary.cc)

set(HEADERS managedsecondary.h virtualsecondary.h)

add_library(virtual_secondary STATIC ${SOURCES})

target_link_libraries(virtual_secondary PUBLIC JsonCpp::JsonCpp)

target_include_directories(virtual_secondary PUBLIC ${PROJECT_SOURCE_DIR}/src/virtual_secondary)

add_aktualizr_test(NAME virtual_secondary SOURCES virtual_secondary_test.cc PROJECT_WORKING_DIRECTORY LIBRARIES uptane_generator_lib)
target_link_libraries(t_virtual_secondary virtual_secondary)

aktualizr_source_file_checks(${HEADERS} ${SOURCES} ${TEST_SOURCES})
