set(SOURCES
    directorrepository.cc
    fetcher.cc
    imagerepository.cc
    iterator.cc
    manifest.cc
    metawithkeys.cc
    role.cc
    root.cc
    secondary_metadata.cc
    tuf.cc
    uptanerepository.cc)

set(HEADERS
    directorrepository.h
    ../../../include/libaktualizr/uptane/exceptions.h
    ../../../include/libaktualizr/uptane/fetcher.h
    ../../../include/libaktualizr/uptane/imagerepository.h
    iterator.h
    manifest.h
    secondary_metadata.h
    ../../../include/libaktualizr/uptane/tuf.h
    ../../../include/libaktualizr/uptane/uptanerepository.h)


add_library(uptane OBJECT ${SOURCES})
target_link_libraries(uptane PUBLIC PkgConfig::JsonCpp)

add_aktualizr_test(NAME tuf SOURCES tuf_test.cc PROJECT_WORKING_DIRECTORY)

if(BUILD_OSTREE AND SOTA_PACKED_CREDENTIALS)
    add_aktualizr_test(NAME uptane_ci SOURCES uptane_ci_test.cc PROJECT_WORKING_DIRECTORY
                        ARGS ${SOTA_PACKED_CREDENTIALS} ${PROJECT_BINARY_DIR}/ostree_repo)
    set_tests_properties(test_uptane_ci PROPERTIES LABELS "credentials")
    target_link_libraries(t_uptane_ci virtual_secondary)
else(BUILD_OSTREE AND SOTA_PACKED_CREDENTIALS)
    list(APPEND TEST_SOURCES uptane_ci_test.cc)
endif(BUILD_OSTREE AND SOTA_PACKED_CREDENTIALS)


add_aktualizr_test(NAME uptane
                   SOURCES uptane_test.cc
                   PROJECT_WORKING_DIRECTORY
                   LIBRARIES uptane_generator_lib provisioner_test_utils virtual_secondary)
set_property(SOURCE uptane_test.cc
             PROPERTY COMPILE_DEFINITIONS
             TEST_PKCS11_MODULE_PATH="${TEST_PKCS11_MODULE_PATH}")
set_tests_properties(test_uptane PROPERTIES LABELS "crypto")

add_aktualizr_test(NAME uptane_delegation SOURCES uptane_delegation_test.cc PROJECT_WORKING_DIRECTORY
                   ARGS "$<TARGET_FILE:uptane-generator>" LIBRARIES uptane_generator_lib)
add_dependencies(t_uptane_delegation uptane-generator)
target_link_libraries(t_uptane_delegation virtual_secondary)
set_tests_properties(test_uptane_delegation PROPERTIES LABELS "crypto")

add_aktualizr_test(NAME uptane_network
                   SOURCES uptane_network_test.cc
                   PROJECT_WORKING_DIRECTORY
                   LIBRARIES uptane_generator_lib virtual_secondary)
set_tests_properties(test_uptane_network PROPERTIES LABELS "crypto")

add_aktualizr_test(NAME uptane_serial SOURCES uptane_serial_test.cc
                   PROJECT_WORKING_DIRECTORY
                   LIBRARIES uptane_generator_lib virtual_secondary)

if(BUILD_OSTREE)
    # Test that SotaUptaneClient::AssembleManifest() works correctly under OSTree.
    # This requires the OSTree sysroot created by make_ostree_sysroot in
    # package_manager/CMakeLists.txt, which is run as a dependency of the
    # build_tests target.
    add_aktualizr_test(NAME uptane_ostree SOURCES uptane_ostree_test.cc
                      PROJECT_WORKING_DIRECTORY
                      ARGS ${PROJECT_BINARY_DIR}/ostree_repo
                      LIBRARIES uptane_generator_lib virtual_secondary)
else(BUILD_OSTREE)
    list(APPEND TEST_SOURCES uptane_ostree_test.cc)
endif(BUILD_OSTREE)

add_aktualizr_test(NAME director SOURCES director_test.cc
                   PROJECT_WORKING_DIRECTORY
                   ARGS "$<TARGET_FILE:uptane-generator>")

aktualizr_source_file_checks(${SOURCES} ${HEADERS} ${TEST_SOURCES})
