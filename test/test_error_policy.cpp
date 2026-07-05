// cpp_tk::Error / cpp_tk::ErrorPolicy(STRICT/LENIENT_CALL/LENIENT_THREAD)の回帰テスト。
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "cpp_tk.hpp"

#include <thread>

namespace tk = cpp_tk;

TEST_CASE("STRICT(既定): 未初期化ウィジェットへのアクセスはErrorを送出する")
{
    tk::set_error_policy(tk::ErrorPolicy::DEFAULT);
    tk::Button uninit_btn;
    CHECK_THROWS_AS(uninit_btn.text("x"), const tk::Error&);
}

TEST_CASE("STRICT(既定): 存在しないTclオプションを渡すconfig()はErrorを送出する")
{
    tk::set_error_policy(tk::ErrorPolicy::DEFAULT);
    tk::Tk root;
    root.withdraw();
    tk::Button btn(root);

    CHECK_THROWS_AS(btn.config({{"no_such_option_xyz", "1"}}), const tk::Error&);
}

TEST_CASE("bool* successを渡す呼び出しはポリシーに関わらず例外を投げず、falseで継続する")
{
    tk::set_error_policy(tk::ErrorPolicy::DEFAULT);
    tk::Tk root;
    root.withdraw();
    tk::Button btn(root);

    auto ret = btn.cget("no_such_option_xyz");
    CHECK(ret.empty());
}

TEST_CASE("LENIENT_CALL: config()失敗時も例外を投げず継続する")
{
    tk::set_error_policy(tk::ErrorPolicy::LENIENT_CALL);
    tk::Tk root;
    root.withdraw();
    tk::Button btn(root);

    CHECK_NOTHROW(btn.config({{"no_such_option_xyz", "1"}}));

    tk::set_error_policy(tk::ErrorPolicy::DEFAULT);
}

TEST_CASE("LENIENT_CALL単体ではクロススレッドアクセスは依然Errorになる(カテゴリ独立)")
{
    tk::set_error_policy(tk::ErrorPolicy::LENIENT_CALL);
    tk::Tk root;
    root.withdraw();
    tk::Button btn(root);

    bool threw = false;
    std::thread([&]() {
        try { btn.text("x"); }
        catch (const tk::Error&) { threw = true; }
    }).join();
    CHECK(threw);

    tk::set_error_policy(tk::ErrorPolicy::DEFAULT);
}

TEST_CASE("LENIENT_THREAD: 別スレッドからの呼び出しは例外を投げず継続し、メインスレッドは影響を受けない")
{
    tk::set_error_policy(tk::ErrorPolicy::LENIENT_THREAD);
    tk::Tk root;
    root.withdraw();
    tk::Button btn(root);
    btn.text("initial");

    std::thread([&]() {
        btn.text("from worker, should not throw");
    }).join();

    // クロススレッド呼び出しは無視され、メインスレッドの値は変わらず、Interpreterも壊れていないこと。
    CHECK(btn.cget("text") == "initial");

    tk::set_error_policy(tk::ErrorPolicy::DEFAULT);
}

TEST_CASE("LENIENT_THREAD単体ではconfig()の失敗は依然Errorになる(カテゴリ独立)")
{
    tk::set_error_policy(tk::ErrorPolicy::LENIENT_THREAD);
    tk::Tk root;
    root.withdraw();
    tk::Button btn(root);

    CHECK_THROWS_AS(btn.config({{"no_such_option_xyz", "1"}}), const tk::Error&);

    tk::set_error_policy(tk::ErrorPolicy::DEFAULT);
}

TEST_CASE("LENIENT_CALL | LENIENT_THREAD: 両方とも例外を投げない")
{
    tk::set_error_policy(tk::ErrorPolicy::LENIENT_CALL | tk::ErrorPolicy::LENIENT_THREAD);
    tk::Tk root;
    root.withdraw();
    tk::Button btn(root);

    CHECK_NOTHROW(btn.config({{"no_such_option_xyz", "1"}}));
    std::thread([&]() { btn.text("from worker"); }).join();

    tk::set_error_policy(tk::ErrorPolicy::DEFAULT);
}
