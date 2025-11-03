find_path(Tk_INCLUDE_PATH tk.h)
find_library(Tk_LIBRARY NAMES tk)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Tk DEFAULT_MSG Tk_LIBRARY Tk_INCLUDE_PATH)

if(Tk_FOUND)
    set(Tk_LIBRARIES ${Tk_LIBRARY})
    set(Tk_INCLUDE_DIRS ${Tk_INCLUDE_PATH})
endif()
