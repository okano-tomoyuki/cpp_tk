find_path(TCL_INCLUDE_PATH
    NAMES tcl.h
    PATH_SUFFIXES tcl8.6 tcl8.5 tcl8.4
)
find_library(TCL_LIBRARY
    NAMES tcl tcl9.0 tcl90 tcl8.7 tcl87 tcl8.6 tcl86 tcl8.5 tcl85 tcl8.4 tcl84
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(TCL DEFAULT_MSG TCL_LIBRARY TCL_INCLUDE_PATH)

if(TCL_FOUND)
    set(TCL_LIBRARIES ${TCL_LIBRARY})
    set(TCL_INCLUDE_DIRS ${TCL_INCLUDE_PATH})
endif()
