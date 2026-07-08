// Regression tests for InterpreterClient::call() (the escape hatch), its public exposure across
// the Widget/PhotoImage/font::Font/ttk::Style/Var classes, and uninitialized-access detection.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "cpp_tk.hpp"

namespace tk  = cpp_tk;
namespace ttk = tk::ttk;

TEST_CASE("call() is public and callable on all 5 classes (escape hatch)")
{
    tk::set_error_policy(tk::ErrorPolicy::DEFAULT);
    tk::Tk root;
    root.withdraw();

    tk::Button btn(root);
    btn.pack();
    bool ok = false;
    auto exists = btn.call({"winfo", "exists", btn.full_name()}, &ok);
    CHECK(ok);
    CHECK(exists == "1");

    tk::PhotoImage img;
    ok = false;
    img.call({"image", "width", img.name()}, &ok);
    CHECK(ok);

    tk::font::Font font;
    ok = false;
    font.call({"font", "actual", font.name()}, &ok);
    CHECK(ok);

    ttk::Style style;
    ok = false;
    style.call({"ttk::style", "theme", "use"}, &ok);
    CHECK(ok);

    tk::StringVar sv;
    ok = false;
    sv.call({"set", sv.name(), "direct"}, &ok);
    CHECK(ok);
    CHECK(sv.get() == "direct");
}

// Regression tests for "call() on an uninitialized object" live in test_uninitialized_objects.cpp
// instead. PhotoImage/font::Font/ttk::Style/Var no longer take a parent and instead bind
// implicitly to current_interp(), so in this file - where another TEST_CASE has already
// constructed a tk::Tk in the same process - current_interp() would already be non-null,
// making it impossible to exercise the intended "uninitialized" state. See docs/tasks.md.

TEST_CASE("ArgValue(Var&): a Var can be passed directly into config (no need to write name())")
{
    tk::set_error_policy(tk::ErrorPolicy::DEFAULT);
    tk::Tk root;
    root.withdraw();

    tk::BooleanVar bv;
    tk::Menu menu(root);
    CHECK_NOTHROW(menu.add_checkbutton({{"label", "check"}, {"variable", bv}}));

    tk::StringVar sv;
    tk::Entry entry(root);
    entry.config({{"textvariable", sv}});
    sv.set("via-config");
    CHECK(entry.get() == "via-config");
}

TEST_CASE("Entry::textvariable / Combobox::set/get work correctly without an internal text_var_ (regression)")
{
    tk::set_error_policy(tk::ErrorPolicy::DEFAULT);
    tk::Tk root;
    root.withdraw();

    tk::StringVar sv;
    tk::Entry entry(root);
    entry.textvariable(sv);
    sv.set("hello");
    CHECK(entry.get() == "hello");
    entry.erase("0", "end");
    entry.insert("0", "world");
    CHECK(sv.get() == "world"); // widget-side changes are reflected back into the Var (Tk's automatic sync)

    ttk::Combobox combo(root);
    combo.values({"a", "b", "c"});
    combo.set("b");
    CHECK(combo.get() == "b");
}
