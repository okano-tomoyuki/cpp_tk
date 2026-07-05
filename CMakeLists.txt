cmake_minimum_required(VERSION 3.10)
project(cpp_tk VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/lib)

# AddressSanitizer/UndefinedBehaviorSanitizerを有効にしたテスト・デバッグ用ビルド。
# ユーザコールバックがhandle()規約を破ってダングリングポインタを参照した場合、通常ビルドでは
# SIGSEGVで落ちるか静かに誤動作するだけで検知できないが(docs/tasks.md C節8.参照)、
# このオプションを有効にすると発生箇所・確保/解放箇所を含む診断付きで確実に検知できる。
# 別のビルドディレクトリで`cmake -S . -B build-asan -DCPP_TK_ENABLE_SANITIZERS=ON`のように使う
# (通常ビルドと同じビルドディレクトリを混在させない)。
option(CPP_TK_ENABLE_SANITIZERS "Build with AddressSanitizer/UndefinedBehaviorSanitizer instrumentation" OFF)
if(CPP_TK_ENABLE_SANITIZERS)
    set(CPP_TK_SANITIZE_FLAGS "-fsanitize=address,undefined -fno-omit-frame-pointer -g")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CPP_TK_SANITIZE_FLAGS}")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${CPP_TK_SANITIZE_FLAGS}")
endif()

add_library(${PROJECT_NAME}
    ${PROJECT_NAME}.cpp
    custom.cpp
)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
find_package(Tk REQUIRED)
find_package(TCL REQUIRED)

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