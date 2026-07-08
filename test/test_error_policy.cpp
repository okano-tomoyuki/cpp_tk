// Regression tests for cpp_tk::Error / cpp_tk::ErrorPolicy (STRICT/LENIENT_CALL/LENIENT_THREAD).
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "cpp_tk.hpp"

#include <thread>

namespace tk = cpp_tk;

TEST_CASE("STRICT (default): accessing an uninitialized widget throws Error")
{
    tk::set_error_policy(tk::ErrorPolicy::DEFAULT);
    tk::Button uninit_btn;
    CHECK_THROWS_AS(uninit_btn.text("x"), const tk::Error&);
}

TEST_CASE("STRICT (default): config() with a nonexistent Tcl option throws Error")
{
    tk::set_error_policy(tk::ErrorPolicy::DEFAULT);
    tk::Tk root;
    root.withdraw();
    tk::Button btn(root);

    CHECK_THROWS_AS(btn.config({{"no_such_option_xyz", "1"}}), const tk::Error&);
}

TEST_CASE("Calls that pass bool* success never throw regardless of policy, and continue with false")
{
    tk::set_error_policy(tk::ErrorPolicy::DEFAULT);
    tk::Tk root;
    root.withdraw();
    tk::Button btn(root);

    auto ret = btn.cget("no_such_option_xyz");
    CHECK(ret.empty());
}

TEST_CASE("LENIENT_CALL: config() failure does not throw and execution continues")
{
    tk::set_error_policy(tk::ErrorPolicy::LENIENT_CALL);
    tk::Tk root;
    root.withdraw();
    tk::Button btn(root);

    CHECK_NOTHROW(btn.config({{"no_such_option_xyz", "1"}}));

    tk::set_error_policy(tk::ErrorPolicy::DEFAULT);
}

TEST_CASE("LENIENT_CALL alone still throws Error for cross-thread access (categories are independent)")
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

TEST_CASE("LENIENT_THREAD: calls from another thread do not throw and continue, main thread is unaffected")
{
    tk::set_error_policy(tk::ErrorPolicy::LENIENT_THREAD);
    tk::Tk root;
    root.withdraw();
    tk::Button btn(root);
    btn.text("initial");

    std::thread([&]() {
        btn.text("from worker, should not throw");
    }).join();

    // The cross-thread call is ignored: the main thread's value is unchanged and the
    // Interpreter is not left in a broken state.
    CHECK(btn.cget("text") == "initial");

    tk::set_error_policy(tk::ErrorPolicy::DEFAULT);
}

TEST_CASE("LENIENT_THREAD alone still throws Error for config() failures (categories are independent)")
{
    tk::set_error_policy(tk::ErrorPolicy::LENIENT_THREAD);
    tk::Tk root;
    root.withdraw();
    tk::Button btn(root);

    CHECK_THROWS_AS(btn.config({{"no_such_option_xyz", "1"}}), const tk::Error&);

    tk::set_error_policy(tk::ErrorPolicy::DEFAULT);
}

TEST_CASE("LENIENT_CALL | LENIENT_THREAD: neither category throws")
{
    tk::set_error_policy(tk::ErrorPolicy::LENIENT_CALL | tk::ErrorPolicy::LENIENT_THREAD);
    tk::Tk root;
    root.withdraw();
    tk::Button btn(root);

    CHECK_NOTHROW(btn.config({{"no_such_option_xyz", "1"}}));
    std::thread([&]() { btn.text("from worker"); }).join();

    tk::set_error_policy(tk::ErrorPolicy::DEFAULT);
}
