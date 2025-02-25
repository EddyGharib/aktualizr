set(CERT_PROVIDER_SRC main.cc)

# set the name of the executable
add_executable(aktualizr-cert-provider ${CERT_PROVIDER_SRC}
    $<TARGET_OBJECTS:bootstrap>
    $<TARGET_OBJECTS:config>
    $<TARGET_OBJECTS:crypto>
    $<TARGET_OBJECTS:http>
    $<TARGET_OBJECTS:logging>
    $<TARGET_OBJECTS:primary_config>
    $<TARGET_OBJECTS:utilities>)

target_link_libraries(aktualizr-cert-provider
    Threads::Threads
    ${Boost_SYSTEM_LIBRARIES}
    ${Boost_LIBRARIES}
    PkgConfig::JsonCpp
    LibArchive::LibArchive
    CURL::libcurl
    OpenSSL::SSL
    sodium::sodium
)

if(BUILD_P11)
    target_link_libraries(aktualizr-cert-provider libp11::libp11)
endif(BUILD_P11)

add_dependencies(build_tests aktualizr-cert-provider)

install(TARGETS aktualizr-cert-provider RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT garage_deploy)

add_aktualizr_test(NAME aktualizr_cert_provider SOURCES cert_provider_test.cc PROJECT_WORKING_DIRECTORY ARGS $<TARGET_FILE:aktualizr-cert-provider>)

if (SOTA_PACKED_CREDENTIALS)
    add_aktualizr_test(NAME aktualizr_cert_provider_shared_cred SOURCES cert_provider_shared_cred_test.cc
        PROJECT_WORKING_DIRECTORY ARGS $<TARGET_FILE:aktualizr-cert-provider> ${SOTA_PACKED_CREDENTIALS})
    set_tests_properties(test_aktualizr_cert_provider_shared_cred PROPERTIES LABELS "credentials")
endif(SOTA_PACKED_CREDENTIALS)

# Check the --help option works.
add_test(NAME aktualizr-cert-provider-option-help
         COMMAND aktualizr-cert-provider --help)

# Report version.
add_test(NAME aktualizr-cert-provider-option-version
         COMMAND aktualizr-cert-provider --version)
set_tests_properties(aktualizr-cert-provider-option-version PROPERTIES PASS_REGULAR_EXPRESSION "Current aktualizr-cert-provider version is: ${AKTUALIZR_VERSION}")

aktualizr_source_file_checks(${CERT_PROVIDER_SRC}
                             cert_provider_shared_cred_test.cc
                             cert_provider_test.cc
                             cert_provider_test.h)

# vim: set tabstop=4 shiftwidth=4 expandtab:
