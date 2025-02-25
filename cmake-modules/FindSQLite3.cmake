# Copyright (C) 2007-2009 LuaDist.
# Created by Peter Kapec <kapecp@gmail.com>
# Redistribution and use of this file is allowed according to the terms of the MIT license.
# For details see the COPYRIGHT file distributed with LuaDist.
#	Note:
#		Searching headers and libraries is very simple and is NOT as powerful as scripts
#		distributed with CMake, because LuaDist defines directories to search for.
#		Everyone is encouraged to contact the author with improvements. Maybe this file
#		becomes part of CMake distribution sometimes.

# - Find sqlite3
# Find the native SQLITE3 headers and libraries.
#
# SQLITE3_INCLUDE_DIRS	- where to find sqlite3.h, etc.
# SQLITE3_LIBRARIES	- List of libraries when using sqlite.
# SQLITE3_FOUND	- True if sqlite found.

# Look for the header file.
FIND_PATH(SQLITE3_INCLUDE_DIR NAMES sqlite3.h)

# Look for the library.
FIND_LIBRARY(SQLITE3_LIBRARY NAMES sqlite3 sqlite)

# Handle the QUIETLY and REQUIRED arguments and set SQLITE3_FOUND to TRUE if all listed variables are TRUE.
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(SQLite3 DEFAULT_MSG SQLITE3_LIBRARY SQLITE3_INCLUDE_DIR)

# Copy the results to the output variables.
IF(SQLITE3_FOUND)
	SET(SQLITE3_LIBRARIES ${SQLITE3_LIBRARY})
	SET(SQLITE3_INCLUDE_DIRS ${SQLITE3_INCLUDE_DIR})
ELSE(SQLITE3_FOUND)
	SET(SQLITE3_LIBRARIES)
	SET(SQLITE3_INCLUDE_DIRS)
ENDIF(SQLITE3_FOUND)

MARK_AS_ADVANCED(SQLITE3_INCLUDE_DIRS SQLITE3_LIBRARIES)

if(NOT TARGET sqlite3)
    add_library(sqlite3 SHARED IMPORTED)
endif()
add_library(sqlite3::sqlite3 ALIAS sqlite3)

message(STATUS "sqlite3::sqlite3::INTERFACE_INCLUDE_DIRECTORIES ${SQLITE3_INCLUDE_DIRS}")
message(STATUS "sqlite3::sqlite3::IMPORTED_LOCATION ${SQLITE3_LIBRARY}")
message(STATUS "sqlite3::sqlite3::INTERFACE_LINK_LIBRARIES ${SQLITE3_LIBRARIES}")
set_target_properties(sqlite3 PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${SQLITE3_INCLUDE_DIRS}")
set_target_properties(sqlite3 PROPERTIES IMPORTED_LOCATION "${SQLITE3_LIBRARY}")
set_target_properties(sqlite3 PROPERTIES INTERFACE_LINK_LIBRARIES "${SQLITE3_LIBRARIES}")