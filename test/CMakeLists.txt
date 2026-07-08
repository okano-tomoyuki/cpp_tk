# Test suite built on doctest (single-header, MIT license, vendored at test/doctest.h).
# Each test file defines its own main() via DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN and becomes a
# separate executable + CTest entry (one process per test file, to avoid the untested risk of
# calling Tk_Init() more than once in the same process).
foreach(test_name
    test_error_policy
    test_interpreter_client
    test_thread_safety
    test_widget_basics
    test_tkinter_parity_batch1
    test_tkinter_parity_batch2
    test_new_classes
    test_native_misc
    test_low_priority_parity_gaps
    test_high_impact_parity_gaps
    test_simpledialog_return
    test_simpledialog_escape
    test_construction_time_options
    test_uninitialized_objects
    test_sv_ttk_theme
    test_calendar
    test_argvalue_composition
)
    add_executable(${test_name}
        ${test_name}.cpp
    )

    target_include_directories(${test_name}
        PRIVATE
            ${PROJECT_SOURCE_DIR}
            ${CMAKE_CURRENT_SOURCE_DIR}
    )

    target_link_libraries(${test_name}
        PRIVATE
            ${PROJECT_NAME}
    )

    cpp_tk_configure_target(${test_name} test)

    add_test(
        NAME ${test_name}
        COMMAND ${test_name}
    )
endforeach()

# Regression test for set_runtime_library_paths() (docs/tasks.md section H). Copies the Tcl/Tk
# runtime script directories detected by cmake/CppTkRuntime.cmake into a build-tree fixture
# located elsewhere from the system's own location, then verifies that Tcl_Init()/Tk_Init()
# succeed using only that fixture (the test itself skips if detection failed).
if(CPP_TK_RUNTIME_TCL_LIBRARY AND CPP_TK_RUNTIME_TK_LIBRARY)
    set(CPP_TK_TEST_RUNTIME_FIXTURE_AVAILABLE 1)
    set(CPP_TK_TEST_FIXTURE_TCL_LIBRARY "${CMAKE_CURRENT_BINARY_DIR}/runtime_fixture/tcl_lib")
    set(CPP_TK_TEST_FIXTURE_TK_LIBRARY  "${CMAKE_CURRENT_BINARY_DIR}/runtime_fixture/tk_lib")
    file(COPY "${CPP_TK_RUNTIME_TCL_LIBRARY}/" DESTINATION "${CPP_TK_TEST_FIXTURE_TCL_LIBRARY}")
    file(COPY "${CPP_TK_RUNTIME_TK_LIBRARY}/"  DESTINATION "${CPP_TK_TEST_FIXTURE_TK_LIBRARY}")
else()
    set(CPP_TK_TEST_RUNTIME_FIXTURE_AVAILABLE 0)
    set(CPP_TK_TEST_FIXTURE_TCL_LIBRARY "")
    set(CPP_TK_TEST_FIXTURE_TK_LIBRARY "")
endif()

add_executable(test_runtime_library_override
    test_runtime_library_override.cpp
)

target_include_directories(test_runtime_library_override
    PRIVATE
        ${PROJECT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_definitions(test_runtime_library_override
    PRIVATE
        CPP_TK_TEST_RUNTIME_FIXTURE_AVAILABLE=${CPP_TK_TEST_RUNTIME_FIXTURE_AVAILABLE}
        CPP_TK_TEST_FIXTURE_TCL_LIBRARY="${CPP_TK_TEST_FIXTURE_TCL_LIBRARY}"
        CPP_TK_TEST_FIXTURE_TK_LIBRARY="${CPP_TK_TEST_FIXTURE_TK_LIBRARY}"
)

target_link_libraries(test_runtime_library_override
    PRIVATE
        ${PROJECT_NAME}
)

cpp_tk_configure_target(test_runtime_library_override test)

add_test(
    NAME test_runtime_library_override
    COMMAND test_runtime_library_override
)

add_subdirectory(compile_check)
