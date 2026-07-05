# docs/todo.md「Var&/StringVar&等が右辺値を拒否することのコンパイル失敗テスト」対応。
# 通常の実行時テスト(doctest)では「コンパイルが失敗すること」自体を検証できないため、
# ここだけは別の仕組みを使う。cpp_tk.hppはTcl/Tk非依存のpimplヘッダ(#includeは標準ライブラリのみ)
# のため、いずれも-c(コンパイルのみ、リンクしない)で完結し、cpp_tkライブラリ本体やTcl/Tkへの
# リンクは不要。
#
# should_compile/ : 名前付きlvalueでの正しい呼び出し(対照群)。通常のOBJECTライブラリとして
#                    cmake --build に含めるだけでよい(コンパイルが失敗すればビルド自体が落ちる)。
# should_fail/     : 一時オブジェクト(右辺値)を渡す誤った呼び出し。1ファイル1API。
#                    通常のビルド対象には含めず、CTestからコンパイラを個別に起動し、
#                    WILL_FAIL TRUEで「失敗すればテスト成功」と判定する。

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
