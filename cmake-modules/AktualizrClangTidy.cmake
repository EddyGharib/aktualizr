if(CLANG_TIDY)
    if(NOT TARGET clang-tidy)
    add_custom_target(clang-tidy)
    endif()
    add_dependencies(qa clang-tidy)
    function(aktualizr_clang_tidy)
        file(RELATIVE_PATH SUBDIR ${CMAKE_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR})
        foreach(FILE ${ARGN})
            string(REPLACE "/" "_" TARGETNAME "aktualizr_clang_tidy-${SUBDIR}-${FILE}")
            add_custom_target(${TARGETNAME}
                    COMMAND ${PROJECT_SOURCE_DIR}/scripts/clang-tidy-wrapper.sh ${CLANG_TIDY} ${CMAKE_BINARY_DIR} ${CMAKE_SOURCE_DIR} ${FILE}
                    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                    VERBATIM)
            add_dependencies(clang-tidy ${TARGETNAME})
        endforeach()
    endfunction()
else()
    message(WARNING "Unable to find clang-tidy-12, clang-tidy-11, or clang-tidy-10; skipping")
    function(aktualizr_clang_tidy)
    endfunction()
endif()
