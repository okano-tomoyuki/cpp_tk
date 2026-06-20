find_path(Tk_INCLUDE_PATH
    NAMES tk.h
    PATH_SUFFIXES tcl8.6 tcl8.5 tcl8.4
)
find_library(Tk_LIBRARY
    NAMES tk tk9.0 tk90 tk8.7 tk87 tk8.6 tk86 tk8.5 tk85 tk8.4 tk84
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Tk DEFAULT_MSG Tk_LIBRARY Tk_INCLUDE_PATH)

if(Tk_FOUND)
    set(Tk_LIBRARIES ${Tk_LIBRARY})
    set(Tk_INCLUDE_DIRS ${Tk_INCLUDE_PATH})
endif()
