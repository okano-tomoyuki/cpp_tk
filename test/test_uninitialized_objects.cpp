// Regression tests for the lazy creation of current_interp(). Kept separate from the other test
// files because this process must never construct a tk::Tk before these run (once a tk::Tk is
// constructed, its Interpreter stays alive for the rest of the process, making it impossible to
// reproduce the "nothing constructed yet" state).
//
// Var/PhotoImage/font::Font/ttk::Style no longer take a parent; instead, at construction they
// bind to current_interp(), which lazily creates an Interpreter for the calling thread if none
// exists yet. So these objects always become real even if a tk::Tk was never constructed (see
// docs/tasks.md). Widget-derived classes, on the other hand, remain a placeholder unless a real
// parent is passed, and are unaffected by this lazy-init behavior.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "cpp_tk.hpp"

namespace tk  = cpp_tk;
namespace ttk = tk::ttk;

TEST_CASE("A Widget constructed without a parent stays a placeholder and call() fails, even with no tk::Tk ever constructed")
{
    tk::set_error_policy(tk::ErrorPolicy::DEFAULT);

    tk::Button uninit_btn;
    bool ok = true;
    auto ret = uninit_btn.call({"winfo", "exists", "."}, &ok);
    CHECK_FALSE(ok);
    CHECK(ret.empty());
}

TEST_CASE("Var/PhotoImage/font::Font/ttk::Style become real automatically via current_interp(), even with no tk::Tk ever constructed")
{
    tk::set_error_policy(tk::ErrorPolicy::DEFAULT);

    tk::PhotoImage img;
    bool ok = false;
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

    tk::StringVar var;
    CHECK(var.get() == "");
}
