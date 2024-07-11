if(CSPELL)
    if(NOT TARGET cspell)
        add_custom_target(cspell)
    endif()
    add_dependencies(qa cspell)
    function(aktualizr_cspell)
        file(RELATIVE_PATH SUBDIR ${CMAKE_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR})
        foreach(FILE ${ARGN})
            string(REPLACE "/" "_" TARGETNAME "aktualizr_cspell-${SUBDIR}-${FILE}")
            add_custom_target(${TARGETNAME}
                    COMMAND ${CSPELL} lint ${FILE} --config ${CMAKE_SOURCE_DIR}/cspell.json --verbose --show-suggestions --unique --gitignore
                    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                    VERBATIM)
            add_dependencies(cspell ${TARGETNAME})
        endforeach()
    endfunction()
else()
    message(WARNING "Unable to find cspell; skipping")
    function(aktualizr_cspell)
    endfunction()
endif()
