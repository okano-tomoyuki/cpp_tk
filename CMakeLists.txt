cmake_minimum_required(VERSION 3.10)
project(cpp_tk VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/lib)

# Test/debug build with AddressSanitizer/UndefinedBehaviorSanitizer instrumentation enabled.
# If a user callback breaks the handle() convention and dereferences a dangling pointer, a
# normal build either crashes with SIGSEGV or silently misbehaves without any way to detect it
# (see docs/tasks.md section C.8), but enabling this option reliably catches it with diagnostics
# including the allocation/deallocation sites. Use it in a separate build directory, e.g.
# `cmake -S . -B build-asan -DCPP_TK_ENABLE_SANITIZERS=ON` (do not mix it into the same build
# directory as a normal build).
option(CPP_TK_ENABLE_SANITIZERS "Build with AddressSanitizer/UndefinedBehaviorSanitizer instrumentation" OFF)
if(CPP_TK_ENABLE_SANITIZERS)
    set(CPP_TK_SANITIZE_FLAGS "-fsanitize=address,undefined -fno-omit-frame-pointer -g")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CPP_TK_SANITIZE_FLAGS}")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${CPP_TK_SANITIZE_FLAGS}")
endif()

add_library(${PROJECT_NAME}
    ${PROJECT_NAME}.cpp
    custom.cpp
    thirdparty/sv_ttk/sv_ttk_data.cpp
)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
find_package(Tk REQUIRED)
find_package(TCL REQUIRED)
include(CppTkRuntime)

# Copy the Tcl/Tk runtime script directory (init.tcl/tk.tcl etc., see docs/tasks.md section H)
# next to the build output for distribution. Set to OFF to skip this if not needed.
option(CPP_TK_PACKAGE_RUNTIME_SCRIPTS "Copy Tcl/Tk runtime script directories next to build output for distribution" ON)
if(CPP_TK_PACKAGE_RUNTIME_SCRIPTS)
    cpp_tk_package_runtime_scripts("${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
endif()

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

add_subdirectory(example)

enable_testing()
add_subdirectory(test)