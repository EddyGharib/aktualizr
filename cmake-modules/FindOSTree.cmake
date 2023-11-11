# Try to find OSTree
# OSTREE_DIR - Hint path to a local build of OSTree
# Once done this will define
#  LIBOSTREE_FOUND
#  LIBOSTREE_INCLUDE_DIRS
#  LIBOSTREE_LIBRARIES
#

# OSTree
find_path(LIBOSTREE_INCLUDE_DIR
    NAMES ostree.h
    HINTS ${OSTREE_DIR}/include
    PATH_SUFFIXES ostree-1 src/libostree)

find_library(LIBOSTREE_LIBRARY
    NAMES libostree-1.so
    HINTS ${OSTREE_DIR}/lib
    PATH_SUFFIXES /.libs)

message("libostree headers path: ${LIBOSTREE_INCLUDE_DIR}")
message("libostree library path: ${LIBOSTREE_LIBRARY}")

# glib
find_package(PkgConfig)

pkg_check_modules(OSTREE_GLIB REQUIRED gio-2.0 glib-2.0)

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(OSTree DEFAULT_MSG
    LIBOSTREE_LIBRARY LIBOSTREE_INCLUDE_DIR OSTREE_GLIB_FOUND)

mark_as_advanced(LIBOSTREE_INCLUDE_DIR LIBOSTREE_LIBRARY)

set(LIBOSTREE_INCLUDE_DIRS ${LIBOSTREE_INCLUDE_DIR} ${OSTREE_GLIB_INCLUDE_DIRS})
set(LIBOSTREE_LIBRARIES ${LIBOSTREE_LIBRARY} ${OSTREE_GLIB_LIBRARIES})

if(NOT TARGET ostree)
    add_library(ostree SHARED IMPORTED)
endif()

set_target_properties(ostree PROPERTIES IMPORTED_GLOBAL TRUE)
add_library(ostree::ostree ALIAS ostree)

message(STATUS "ostree::ostree::INTERFACE_INCLUDE_DIRECTORIES ${LIBOSTREE_INCLUDE_DIRS}")
message(STATUS "ostree::ostree::IMPORTED_LOCATION ${LIBOSTREE_LIBRARY}")
message(STATUS "ostree::ostree::INTERFACE_LINK_LIBRARIES ${LIBOSTREE_LIBRARIES}")
set_target_properties(ostree PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${LIBOSTREE_INCLUDE_DIRS}")
set_target_properties(ostree PROPERTIES IMPORTED_LOCATION "${LIBOSTREE_LIBRARY}")
set_target_properties(ostree PROPERTIES INTERFACE_LINK_LIBRARIES "${LIBOSTREE_LIBRARIES}")
