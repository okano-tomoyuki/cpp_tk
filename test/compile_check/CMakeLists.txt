# Compile-failure tests for "Var&/StringVar& etc. reject rvalues" (see docs/todo.md).
# An ordinary runtime test (doctest) cannot verify that "compilation itself fails", so this uses
# a different mechanism. cpp_tk.hpp is a Tcl/Tk-independent pimpl header (only standard-library
# #includes), so every case here is self-contained under -c (compile only, no link) - none of
# them need to link against the cpp_tk library itself or Tcl/Tk.
#
# should_compile/ : the control group - correct calls using a named lvalue. Just included in
#                    cmake --build as an ordinary OBJECT library (the build itself fails if
#                    compilation fails).
# should_fail/     : incorrect calls passing a temporary object (rvalue). One API per file.
#                    Not part of the normal build; instead CTest invokes the compiler directly
#                    for each one and asserts WILL_FAIL TRUE ("test passes if compilation fails").

add_library(compile_check_should_compile OBJECT
    should_compile/rvalue_reject_control.cpp
)

target_include_directories(compile_check_should_compile
    PRIVATE
        ${PROJECT_SOURCE_DIR}
)

file(GLOB SHOULD_FAIL_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/should_fail/*.cpp)

foreach(src ${SHOULD_FAIL_SOURCES})
    get_filename_component(case_name ${src} NAME_WE)
    set(test_name compile_fail_${case_name})

    add_test(
        NAME ${test_name}
        COMMAND ${CMAKE_CXX_COMPILER} -std=c++11 -I${PROJECT_SOURCE_DIR} -c ${src} -o ${CMAKE_CURRENT_BINARY_DIR}/${case_name}.o
    )
    set_tests_properties(${test_name} PROPERTIES WILL_FAIL TRUE)
endforeach()
