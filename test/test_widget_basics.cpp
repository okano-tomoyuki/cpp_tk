// Regression tests for common Widget functionality (geometry managers/config & cget/bind &
// unbind/after family/winfo family/focus family/clipboard family/wait family/lift & lower).
// None of this had automated tests before, and it is the most fundamental and frequently used
// part of the library, so it is prioritized here.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "cpp_tk.hpp"

namespace tk = cpp_tk;

TEST_CASE("pack/pack_forget: winfo_manager tracks the change")
{
    tk::Tk root;
    root.withdraw();
    tk::Frame f(root);

    CHECK(f.winfo_manager().empty());
    f.pack();
    CHECK(f.winfo_manager() == "pack");
    f.pack_forget();
    CHECK(f.winfo_manager().empty());
}

TEST_CASE("grid/grid_forget: winfo_manager tracks the change")
{
    tk::Tk root;
    root.withdraw();
    tk::Frame f(root);

    f.grid();
    CHECK(f.winfo_manager() == "grid");
    f.grid_forget();
    CHECK(f.winfo_manager().empty());
}

TEST_CASE("place/place_forget: winfo_manager tracks the change")
{
    tk::Tk root;
    root.withdraw();
    tk::Frame f(root);

    f.place({{"x", 0}, {"y", 0}});
    CHECK(f.winfo_manager() == "place");
    f.place_forget();
    CHECK(f.winfo_manager().empty());
}

TEST_CASE("config/cget: a configured value can be read back")
{
    tk::Tk root;
    root.withdraw();
    tk::Label label(root);

    label.config("text", "hello");
    CHECK(label.cget("text") == "hello");

    label.config({{"text", "world"}, {"width", 10}});
    CHECK(label.cget("text") == "world");
    CHECK(label.cget("width") == "10");
}

TEST_CASE("grid_rowconfigure/grid_columnconfigure: do not throw")
{
    tk::Tk root;
    root.withdraw();
    tk::Frame f(root);
    f.pack();

    CHECK_NOTHROW(f.grid_rowconfigure(0, {{"weight", 1}}));
    CHECK_NOTHROW(f.grid_columnconfigure(0, {{"weight", 1}}));

    // root is reused by other TEST_CASEs in the same process; leaving this frame packed would
    // make later TEST_CASEs fight over root's pack area, so destroy it explicitly once done.
    f.destroy();
}

TEST_CASE("bind/unbind: a bound callback fires, and stops firing after unbind")
{
    tk::Tk root;
    // Event delivery to bindings via event_generate only happens while the window is mapped
    // (viewable); it was confirmed on real hardware that nothing is delivered while withdrawn.
    // Instead of withdraw(), place the window off-screen to keep it mapped. If an earlier
    // TEST_CASE already left the window mapped, deiconify() alone produces no state change (no
    // Visibility event), so wait_visibility() would hang; withdraw() first guarantees an actual
    // unmapped state before deiconify() re-maps it, so a change always occurs.
    root.withdraw();
    root.deiconify();
    root.geometry("1x1-3000-3000");
    root.wait_visibility(); // wait for deiconify() to actually take effect (become mapped)
    tk::Frame f(root);
    f.pack();
    f.update(); // force widget creation so bindings become active

    int fired = 0;
    f.bind("<<TestEvent>>", [&](const tk::Event&) { ++fired; });

    f.event_generate("<<TestEvent>>");
    f.update();
    CHECK(fired == 1);

    f.unbind("<<TestEvent>>");
    f.event_generate("<<TestEvent>>");
    f.update();
    CHECK(fired == 1); // unchanged after unbind
}

TEST_CASE("after: the callback runs after the specified number of milliseconds (via mainloop)")
{
    tk::Tk root;
    root.withdraw();

    bool fired = false;
    root.after(10, [&]() {
        fired = true;
        root.quit();
    });
    root.mainloop();

    CHECK(fired);
}

TEST_CASE("after_cancel: a cancelled callback never runs")
{
    tk::Tk root;
    root.withdraw();

    bool cancelled_fired = false;
    auto id = root.after(20, [&]() { cancelled_fired = true; });
    root.after_cancel(id);

    // Schedule a separate quit() timer that fires after the cancelled one would have,
    // then confirm cancelled_fired is still false at that point.
    root.after(60, [&]() { root.quit(); });
    root.mainloop();

    CHECK_FALSE(cancelled_fired);
}

TEST_CASE("after_idle: the callback runs during idle time")
{
    tk::Tk root;
    root.withdraw();

    bool fired = false;
    root.after_idle([&]() {
        fired = true;
        root.quit();
    });
    root.mainloop();

    CHECK(fired);
}

TEST_CASE("destroy/winfo_exists: winfo_exists becomes false after destroy")
{
    tk::Tk root;
    root.withdraw();
    tk::Frame f(root);
    f.pack();

    CHECK(f.winfo_exists());
    f.destroy();
    root.update();
    CHECK_FALSE(f.winfo_exists());
}

TEST_CASE("winfo_class/winfo_toplevel/winfo_children: basic information can be retrieved")
{
    tk::Tk root;
    root.withdraw();
    tk::Frame f(root);
    f.pack();
    tk::Label child(f);
    child.pack();

    CHECK(f.winfo_class() == "Frame");
    CHECK(f.winfo_toplevel() == root.full_name());

    auto children = f.winfo_children();
    CHECK(children.size() == 1);
    CHECK(children[0] == child.full_name());
}

TEST_CASE("winfo_width/height/x/y/rootx/rooty: return integers without throwing")
{
    tk::Tk root;
    root.withdraw();
    tk::Frame f(root);
    f.pack();
    f.update();

    CHECK_NOTHROW(f.winfo_width());
    CHECK_NOTHROW(f.winfo_height());
    CHECK_NOTHROW(f.winfo_x());
    CHECK_NOTHROW(f.winfo_y());
    CHECK_NOTHROW(f.winfo_rootx());
    CHECK_NOTHROW(f.winfo_rooty());
}

TEST_CASE("winfo_screenwidth/screenheight: return positive values")
{
    tk::Tk root;
    root.withdraw();

    CHECK(root.winfo_screenwidth() > 0);
    CHECK(root.winfo_screenheight() > 0);
}

TEST_CASE("winfo_pointerx/pointery: do not throw")
{
    tk::Tk root;
    root.withdraw();

    CHECK_NOTHROW(root.winfo_pointerx());
    CHECK_NOTHROW(root.winfo_pointery());
}

TEST_CASE("winfo_ismapped: stays false while withdrawn")
{
    tk::Tk root;
    root.withdraw();
    tk::Frame f(root);
    f.pack();
    f.update();

    // While the parent root is withdrawn, its children are not mapped either.
    CHECK_FALSE(f.winfo_ismapped());
}

TEST_CASE("clipboard_clear/append/get: a stored string can be read back")
{
    tk::Tk root;
    root.withdraw();

    root.clipboard_clear();
    root.clipboard_append("hello clipboard");
    CHECK(root.clipboard_get() == "hello clipboard");
}

TEST_CASE("wait_variable: blocks until the Var is written, then returns")
{
    tk::Tk root;
    root.withdraw();
    tk::StringVar var;

    // wait_variable() blocks in a nested event loop, so Tcl timer events still fire while
    // blocked. The after() scheduled below fires during that block and updates the var.
    root.after(10, [&]() { var.set("changed"); });
    root.wait_variable(var);

    CHECK(var.get() == "changed");
}

TEST_CASE("wait_window: blocks until the widget is destroyed, then returns")
{
    tk::Tk root;
    root.withdraw();
    tk::Toplevel win(root);

    bool destroyed_before_return = false;
    root.after(10, [&]() {
        win.destroy();
        destroyed_before_return = true;
    });
    win.wait_window();

    CHECK(destroyed_before_return);
}

TEST_CASE("lift/lower: do not throw")
{
    tk::Tk root;
    root.withdraw();
    tk::Frame f1(root);
    tk::Frame f2(root);
    f1.place({{"x", 0}, {"y", 0}});
    f2.place({{"x", 0}, {"y", 0}});

    CHECK_NOTHROW(f1.lift());
    CHECK_NOTHROW(f2.lower());
}

TEST_CASE("as_parent: passing the same concrete type as the parent avoids the compile error via as_parent()")
{
    tk::Tk root;
    root.withdraw();
    tk::Frame parent(root);

    // tk::Frame child(parent); would pick the deleted Frame(const Frame&) (copying is
    // disabled) because C++ always prefers an exact-match overload even when it is deleted
    // (see docs/tasks.md section I). as_parent() avoids this by changing the static type to
    // Widget&.
    tk::Frame child(parent.as_parent());
    child.pack();

    CHECK(child.winfo_parent() == parent.full_name());
}
