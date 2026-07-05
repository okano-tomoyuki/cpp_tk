# doctest(単一ヘッダ、MITライセンス、test/doctest.hにベンダリング済み)を使ったテスト群。
# 各テストファイルはDOCTEST_CONFIG_IMPLEMENT_WITH_MAINで自前main()を持ち、
# 個別の実行ファイル+CTestエントリになる(1プロセス内で複数のTk_Init()を直列に
# 呼ぶ未検証のリスクを避けるため、テストファイルごとにプロセスを分離している)。
foreach(test_name
    test_error_policy
    test_interpreter_client
    test_thread_safety
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

    add_test(
        NAME ${test_name}
        COMMAND ${test_name}
    )
endforeach()
