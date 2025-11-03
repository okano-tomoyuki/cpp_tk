find_path(TCL_INCLUDE_PATH tcl.h)
find_library(TCL_LIBRARY NAMES tcl)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(TCL DEFAULT_MSG TCL_LIBRARY TCL_INCLUDE_PATH)

if(TCL_FOUND)
    set(TCL_LIBRARIES ${TCL_LIBRARY})
    set(TCL_INCLUDE_DIRS ${TCL_INCLUDE_PATH})
endif()
