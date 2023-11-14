
if(CLANG_FORMAT)
    function(aktualizr_clang_format)
        file(RELATIVE_PATH SUBDIR ${CMAKE_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR})
        foreach(FILE ${ARGN})
            string(REPLACE "/" "_" TARGETNAME "aktualizr_clang_format-${SUBDIR}-${FILE}")
            add_custom_target(${TARGETNAME}
                    COMMAND ${CLANG_FORMAT} -i -style=file ${FILE}
                    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                    VERBATIM)
            add_dependencies(format ${TARGETNAME})

            # The check for CI that fails if stuff changes
            string(REPLACE "/" "_" TARGETNAME_CI "aktualizr_ci_clang_format-${SUBDIR}-${FILE}")

            add_custom_target(${TARGETNAME_CI}
                    COMMAND ${CLANG_FORMAT} -style=file ${FILE} | diff -u ${FILE} - || { echo 'Found unformatted code! Run make format'\; false\; }
                    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
            add_dependencies(check-format ${TARGETNAME_CI})
        endforeach()
    endfunction()
else()
    message(WARNING "clang-format-11 not found, skipping")
    function(aktualizr_clang_format)
    endfunction()
endif()
