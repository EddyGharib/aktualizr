if(BUILD_WITH_CODE_COVERAGE)
    add_definitions(${COVERAGE_COMPILER_FLAGS})
endif()

add_subdirectory("libaktualizr")
add_subdirectory("libaktualizr-c")
add_subdirectory("virtual_secondary")
add_subdirectory("sota_tools")
add_subdirectory("aktualizr_primary")
add_subdirectory("libaktualizr-posix")
add_subdirectory("aktualizr_secondary")
add_subdirectory("aktualizr_info")
add_subdirectory("uptane_generator")

add_subdirectory("cert_provider")
add_subdirectory("aktualizr_get")

install(EXPORT aktualizrTargets NAMESPACE aktualizr:: DESTINATION lib/cmake/aktualizr)

include(CMakePackageConfigHelpers)
configure_package_config_file(
        "${CMAKE_SOURCE_DIR}/cmake/aktualizrConfig.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/aktualizrConfig.cmake"
        INSTALL_DESTINATION "lib/cmake/aktualizr"
)

write_basic_package_version_file(
        ${CMAKE_CURRENT_BINARY_DIR}/aktualizrConfigVersion.cmake
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/aktualizrConfig.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/aktualizrConfigVersion.cmake
        DESTINATION "lib/cmake/aktualizr"
)
