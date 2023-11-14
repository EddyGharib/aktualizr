
function(aktualizr_source_file_checks)
    list(REMOVE_DUPLICATES ARGN)
    foreach(FILE ${ARGN})
        file(RELATIVE_PATH FULL_FN ${CMAKE_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/${FILE})
        set(AKTUALIZR_CHECKED_SRCS ${AKTUALIZR_CHECKED_SRCS} ${FULL_FN} CACHE INTERNAL "")
        if(NOT EXISTS ${CMAKE_SOURCE_DIR}/${FULL_FN})
            message(FATAL_ERROR "file ${FULL_FN} does not exist")
        endif()
    endforeach()
    aktualizr_clang_format(${ARGN})

    # exclude test files from clang-tidy because false positives in googletest
    # are hard to remove...
    file(RELATIVE_PATH SUBDIR ${CMAKE_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR})
    if(NOT ${SUBDIR} MATCHES "tests.*")
        foreach(FILE ${ARGN})
            if(NOT ${FILE} MATCHES ".*_test\\..*$")
                list(APPEND filtered_files ${FILE})
            endif()
        endforeach()
        aktualizr_clang_tidy(${filtered_files})
    endif()
endfunction()
