# - Find Orcus
# Find the Orcus filter library.
#
# This module defines
#  Orcus_FOUND - whether the Orcus library was found
#  Orcus_LIBRARIES - the Orcus library
#  Orcus_INCLUDE_DIR - the include path of the Orcus library

# SPDX-FileCopyrightText: 2022 Stefan Gerlach <stefan.gerlach@uni.kn>
# SPDX-License-Identifier: BSD-3-Clause

if (Orcus_INCLUDE_DIR AND Orcus_LIBRARIES)
    # Already in cache
    set (Orcus_FOUND TRUE)
else ()
    find_package(PkgConfig QUIET)
    pkg_check_modules(PC_ORCUS liborcus-0.17 QUIET)
    pkg_check_modules(PC_IXION libixion-0.17 QUIET)

    find_library (Orcus_LIBRARY
        NAMES orcus orcus-0.17
        HINTS ${PC_ORCUS_LIBRARY_DIRS}
        PATH_SUFFIXES orcus
    )
    find_library (Orcus_parser_LIBRARY
        NAMES orcus-parser orcus-parser-0.17
        HINTS ${PC_ORCUS_LIBRARY_DIRS}
        PATH_SUFFIXES orcus
    )
    find_library (Orcus_spreadsheet_LIBRARY
        NAMES orcus-spreadsheet-model orcus-spreadsheet-model-0.17
        HINTS ${PC_ORCUS_LIBRARY_DIRS}
        PATH_SUFFIXES orcus
    )
    set(Orcus_LIBRARIES ${Orcus_LIBRARY} ${Orcus_parser_LIBRARY} ${Orcus_spreadsheet_LIBRARY})

    find_library (Ixion_LIBRARY
        NAMES ixion ixion-0.17
	HINTS ${PC_IXION_LIBRARY_DIRS}
        PATH_SUFFIXES orcus
    )
    find_path (Orcus_INCLUDE_DIR
        NAMES orcus/orcus_ods.hpp
        HINTS ${PC_ORCUS_INCLUDE_DIRS}
        PATH_SUFFIXES orcus
    )
    find_path (Ixion_INCLUDE_DIR
        NAMES ixion/info.hpp
	HINTS ${PC_IXION_INCLUDE_DIRS}
        PATH_SUFFIXES ixion
    )

    include (FindPackageHandleStandardArgs)
    find_package_handle_standard_args (Orcus DEFAULT_MSG Orcus_LIBRARIES Orcus_INCLUDE_DIR Ixion_INCLUDE_DIR)
endif ()

mark_as_advanced(Orcus_INCLUDE_DIR Ixion_INCLUDE_DIR Orcus_LIBRARY Orcus_parser_LIBRARY Orcus_spreadsheet_LIBRARY Orcus_LIBRARIES Ixion_LIBRARY)

if (Orcus_FOUND)
   add_library(Orcus UNKNOWN IMPORTED)
   set_target_properties(Orcus PROPERTIES INTERFACE_INCLUDE_DIRECTORIES ${Orcus_INCLUDE_DIR} IMPORTED_LOCATION "${Orcus_LIBRARIES} ${Ixion_LIBRARY}")
endif()
