SET(SOURCES libaktualizr-c.cc)

add_library(aktualizr-c SHARED ${SOURCES}
    $<TARGET_OBJECTS:asn1>
    $<TARGET_OBJECTS:asn1_lib>
    $<TARGET_OBJECTS:bootloader>
    $<TARGET_OBJECTS:bootstrap>
    $<TARGET_OBJECTS:campaign>
    $<TARGET_OBJECTS:config>
    $<TARGET_OBJECTS:crypto>
    $<TARGET_OBJECTS:http>
    $<TARGET_OBJECTS:package_manager>
    $<TARGET_OBJECTS:primary>
    $<TARGET_OBJECTS:primary_config>
    $<TARGET_OBJECTS:utilities>
    $<TARGET_OBJECTS:logging>
    $<TARGET_OBJECTS:storage>
    $<TARGET_OBJECTS:uptane>)

target_include_directories(aktualizr-c PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(aktualizr-c PRIVATE ${AKTUALIZR_EXTERNAL_LIBS} PUBLIC ${AKTUALIZR_PUBLIC_LIBS})

aktualizr_source_file_checks(${SOURCES})

add_subdirectory(test)
