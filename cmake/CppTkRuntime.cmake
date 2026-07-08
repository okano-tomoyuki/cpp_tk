# Detects where the Tcl/Tk runtime script directory (init.tcl/tk.tcl etc.) lives and whether
# zipfs is supported (see docs/tasks.md section H). find_package(TCL)/find_package(Tk) only
# search for the compiled library and headers, so the script directory referenced at runtime as
# $tcl_library/$tk_library is queried separately by actually launching wish (an interpreter that
# loads both Tcl and Tk).

set(CPP_TK_RUNTIME_TCL_LIBRARY "" CACHE PATH "Detected Tcl runtime script directory ($tcl_library)")
set(CPP_TK_RUNTIME_TK_LIBRARY  "" CACHE PATH "Detected Tk runtime script directory ($tk_library)")
option(CPP_TK_RUNTIME_HAS_ZIPFS "Detected Tcl/Tk zipfs (virtual filesystem) support" OFF)

# A "wish" found on PATH may belong to a different Tcl/Tk install than the one found by
# find_package(TCL)/find_package(Tk) (e.g. the mingw64 bundled with Git for Windows). If they
# disagree, the copied scripts' version would not match the linked libtcl/libtk (exactly the
# concern raised in docs/tasks.md section H). To avoid this, prefer <prefix>/bin derived from
# TCL_LIBRARY's actual location (<prefix>/lib/libtcl...), falling back to a normal PATH search
# only if nothing is found there.
set(_cpp_tk_wish_names wish wish9.0 wish90 wish8.7 wish87 wish8.6 wish86 wish8.5 wish85)
if(TCL_LIBRARY)
    get_filename_component(_cpp_tk_tcl_lib_dir "${TCL_LIBRARY}" DIRECTORY)
    get_filename_component(_cpp_tk_tcl_prefix  "${_cpp_tk_tcl_lib_dir}" DIRECTORY)
    find_program(CPP_TK_WISH_EXECUTABLE
        NAMES ${_cpp_tk_wish_names}
        HINTS "${_cpp_tk_tcl_prefix}/bin"
        NO_DEFAULT_PATH
    )
endif()
if(NOT CPP_TK_WISH_EXECUTABLE)
    find_program(CPP_TK_WISH_EXECUTABLE NAMES ${_cpp_tk_wish_names})
endif()

if(NOT CPP_TK_WISH_EXECUTABLE)
    message(WARNING "cpp_tk: wish was not found, so automatic detection of the Tcl/Tk runtime "
                     "script directory and CPP_TK_PACKAGE_RUNTIME_SCRIPTS are disabled.")
else()
    set(_cpp_tk_query_script "${CMAKE_BINARY_DIR}/cpp_tk_query_runtime.tcl")
    file(WRITE "${_cpp_tk_query_script}" "
wm withdraw .
puts \"TCL_LIBRARY=$tcl_library\"
puts \"TK_LIBRARY=$tk_library\"
puts \"ZIPFS=[llength [info commands ::zipfs]]\"
exit
")

    execute_process(
        COMMAND "${CPP_TK_WISH_EXECUTABLE}" "${_cpp_tk_query_script}"
        OUTPUT_VARIABLE _cpp_tk_query_output
        ERROR_VARIABLE  _cpp_tk_query_error
        RESULT_VARIABLE _cpp_tk_query_result
    )

    if(NOT _cpp_tk_query_result EQUAL 0)
        message(WARNING "cpp_tk: runtime detection via ${CPP_TK_WISH_EXECUTABLE} failed: "
                         "${_cpp_tk_query_error}")
    else()
        if(_cpp_tk_query_output MATCHES "TCL_LIBRARY=([^\r\n]*)")
            set(CPP_TK_RUNTIME_TCL_LIBRARY "${CMAKE_MATCH_1}" CACHE PATH "" FORCE)
        endif()
        if(_cpp_tk_query_output MATCHES "TK_LIBRARY=([^\r\n]*)")
            set(CPP_TK_RUNTIME_TK_LIBRARY "${CMAKE_MATCH_1}" CACHE PATH "" FORCE)
        endif()
        if(_cpp_tk_query_output MATCHES "ZIPFS=([0-9]+)")
            if(CMAKE_MATCH_1 GREATER 0)
                set(CPP_TK_RUNTIME_HAS_ZIPFS ON CACHE BOOL "" FORCE)
            else()
                set(CPP_TK_RUNTIME_HAS_ZIPFS OFF CACHE BOOL "" FORCE)
            endif()
        endif()
    endif()
endif()

message(STATUS "cpp_tk: detected Tcl runtime library dir = ${CPP_TK_RUNTIME_TCL_LIBRARY}")
message(STATUS "cpp_tk: detected Tk runtime library dir  = ${CPP_TK_RUNTIME_TK_LIBRARY}")
message(STATUS "cpp_tk: zipfs (virtual filesystem) support = ${CPP_TK_RUNTIME_HAS_ZIPFS}")
if(NOT CPP_TK_RUNTIME_HAS_ZIPFS)
    message(STATUS "cpp_tk: zipfs was not detected, so single-executable packaging (VFS "
                    "embedding) is not available. Use CPP_TK_PACKAGE_RUNTIME_SCRIPTS's directory "
                    "copy approach for distribution instead (see docs/tasks.md section H).")
endif()

# Copies the detected Tcl/Tk runtime script directories into dest_dir, named tclX.Y/tkX.Y (the
# "place next to the executable" distribution approach, see docs/tasks.md section H). Left as a
# function rather than a new CMake target, so the caller controls which target and build stage
# invokes it.
function(cpp_tk_package_runtime_scripts dest_dir)
    if(NOT CPP_TK_RUNTIME_TCL_LIBRARY OR NOT CPP_TK_RUNTIME_TK_LIBRARY)
        message(WARNING "cpp_tk_package_runtime_scripts: skipping the copy because the runtime "
                         "script directories were not detected.")
        return()
    endif()

    get_filename_component(_cpp_tk_tcl_dir_name "${CPP_TK_RUNTIME_TCL_LIBRARY}" NAME)
    get_filename_component(_cpp_tk_tk_dir_name  "${CPP_TK_RUNTIME_TK_LIBRARY}"  NAME)

    file(COPY "${CPP_TK_RUNTIME_TCL_LIBRARY}" DESTINATION "${dest_dir}")
    file(COPY "${CPP_TK_RUNTIME_TK_LIBRARY}"  DESTINATION "${dest_dir}")

    message(STATUS "cpp_tk: ${CPP_TK_RUNTIME_TCL_LIBRARY} -> ${dest_dir}/${_cpp_tk_tcl_dir_name}")
    message(STATUS "cpp_tk: ${CPP_TK_RUNTIME_TK_LIBRARY} -> ${dest_dir}/${_cpp_tk_tk_dir_name}")
endfunction()
