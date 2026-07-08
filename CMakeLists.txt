cmake_minimum_required(VERSION 3.10)
project(cpp_tk VERSION 0.1 LANGUAGES CXX)

# Output directories for cpp_tk's own targets (its library, examples, and tests). Deliberately
# NOT written to the directory-scoped CMAKE_RUNTIME_OUTPUT_DIRECTORY / CMAKE_LIBRARY_OUTPUT_DIRECTORY
# / CMAKE_ARCHIVE_OUTPUT_DIRECTORY variables: a project that embeds cpp_tk via add_subdirectory()
# should not have its own targets' output locations affected by cpp_tk's choices (or vice versa).
# cpp_tk_SOURCE_DIR (not PROJECT_SOURCE_DIR) is used so this keeps pointing at cpp_tk's own root
# even if a consuming project's CMakeLists.txt calls project() again elsewhere.
set(CPP_TK_RUNTIME_DIR "${cpp_tk_SOURCE_DIR}/bin")
set(CPP_TK_LIBRARY_DIR "${cpp_tk_SOURCE_DIR}/lib")

# Test/debug build with AddressSanitizer/UndefinedBehaviorSanitizer instrumentation enabled.
# If a user callback breaks the handle() convention and dereferences a dangling pointer, a
# normal build either crashes with SIGSEGV or silently misbehaves without any way to detect it
# (see docs/tasks.md section C.8), but enabling this option reliably catches it with diagnostics
# including the allocation/deallocation sites. Use it in a separate build directory, e.g.
# `cmake -S . -B build-asan -DCPP_TK_ENABLE_SANITIZERS=ON` (do not mix it into the same build
# directory as a normal build).
option(CPP_TK_ENABLE_SANITIZERS "Build with AddressSanitizer/UndefinedBehaviorSanitizer instrumentation" OFF)
if(CPP_TK_ENABLE_SANITIZERS)
    set(CPP_TK_SANITIZE_COMPILE_FLAGS -fsanitize=address,undefined -fno-omit-frame-pointer -g)
    set(CPP_TK_SANITIZE_LINK_FLAGS "-fsanitize=address,undefined")
endif()

# Applies cpp_tk's own output-directory layout and (optionally) sanitizer instrumentation to a
# single target, entirely via target properties rather than directory-scoped/global CMake
# variables (CMAKE_CXX_STANDARD, CMAKE_RUNTIME_OUTPUT_DIRECTORY, CMAKE_CXX_FLAGS, etc.). Every
# target this project defines - the library itself, its examples, its tests - calls this, so
# that embedding cpp_tk into another project via add_subdirectory() can never affect that
# project's own targets or build settings.
#
# An optional second argument places the target's runtime output under a subdirectory of
# CPP_TK_RUNTIME_DIR (e.g. cpp_tk_configure_target(sample1 example) -> bin/example/sample1),
# so example and test executables don't all land mixed together directly under bin/.
function(cpp_tk_configure_target target)
    set(_cpp_tk_runtime_dir "${CPP_TK_RUNTIME_DIR}")
    if(ARGC GREATER 1)
        set(_cpp_tk_runtime_dir "${CPP_TK_RUNTIME_DIR}/${ARGV1}")
    endif()
    set_target_properties(${target} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${_cpp_tk_runtime_dir}"
        LIBRARY_OUTPUT_DIRECTORY "${CPP_TK_LIBRARY_DIR}"
        ARCHIVE_OUTPUT_DIRECTORY "${CPP_TK_LIBRARY_DIR}"
    )
    if(CPP_TK_ENABLE_SANITIZERS)
        target_compile_options(${target} PRIVATE ${CPP_TK_SANITIZE_COMPILE_FLAGS})
        set_target_properties(${target} PROPERTIES LINK_FLAGS "${CPP_TK_SANITIZE_LINK_FLAGS}")
    endif()
endfunction()

add_library(${PROJECT_NAME}
    ${PROJECT_NAME}.cpp
    custom.cpp
    thirdparty/sv_ttk/sv_ttk_data.cpp
)
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_11)
cpp_tk_configure_target(${PROJECT_NAME})

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
find_package(Tk REQUIRED)
find_package(TCL REQUIRED)
include(CppTkRuntime)

target_include_directories(${PROJECT_NAME}
    PRIVATE
        ${Tk_INCLUDE_DIRS}
        ${TCL_INCLUDE_DIRS}
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        ${Tk_LIBRARIES}
        ${TCL_LIBRARIES}
)

# Building cpp_tk's own example programs / test suite is opt-out rather than mandatory, so a
# project that embeds cpp_tk via add_subdirectory() can skip either (or both) with
# `-DCPP_TK_BUILD_EXAMPLES=OFF` / `-DCPP_TK_BUILD_TESTS=OFF`. Both default to ON to preserve the
# existing standalone workflow (`cmake -S . -B build && ctest` working out of the box).
option(CPP_TK_BUILD_EXAMPLES "Build cpp_tk's own example programs" ON)
if(CPP_TK_BUILD_EXAMPLES)
    add_subdirectory(example)
endif()

# Copy the Tcl/Tk runtime script directory (init.tcl/tk.tcl etc., see docs/tasks.md section H)
# next to cpp_tk's own example binaries for distribution. Only meaningful when the examples are
# actually built; a downstream project wanting the same for its own executable calls
# cpp_tk_package_runtime_scripts(<its own output dir>) itself.
option(CPP_TK_PACKAGE_RUNTIME_SCRIPTS "Copy Tcl/Tk runtime script directories next to build output for distribution" ON)
if(CPP_TK_PACKAGE_RUNTIME_SCRIPTS AND CPP_TK_BUILD_EXAMPLES)
    cpp_tk_package_runtime_scripts("${CPP_TK_RUNTIME_DIR}/example")
endif()

option(CPP_TK_BUILD_TESTS "Build cpp_tk's own test suite" ON)
if(CPP_TK_BUILD_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()
