// Regression tests for the "bugs" / "structural design issue" sections of docs/todo.md:
// - ttk::Treeview::bbox() does not error when column is omitted
// - Widget construction now embeds options directly into the creation command, so
//   construction-time-only (read-only) options can be set too (ttk::PanedWindow's -orient,
//   Frame's -class/-container)
// - regression for a pre-existing bug found as a byproduct (ttk::Separator was ignoring its options)
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "cpp_tk.hpp"

namespace tk  = cpp_tk;
namespace ttk = tk::ttk;

TEST_CASE("ttk::Treeview::bbox: does not error when column is omitted")
{
    tk::Tk root;
    root.withdraw();

    ttk::Treeview tv(root, {{"columns", "c1"}});
    tv.pack();
    tv.insert("", "end", "item1", {{"text", "row1"}});
    root.update();

    std::vector<int> box;
    CHECK_NOTHROW(box = tv.bbox("item1"));
}

TEST_CASE("ttk::PanedWindow: the construction-time-only -orient option can be set via the constructor")
{
    tk::Tk root;
    root.withdraw();

    ttk::PanedWindow pw(root, {{"orient", "horizontal"}});
    CHECK(pw.cget("orient") == "horizontal");
}

TEST_CASE("Frame: the construction-time-only -class/-container options can be set via the constructor")
{
    tk::Tk root;
    root.withdraw();

    tk::Frame f(root, {{"class", "MyClass"}, {"container", true}});
    CHECK(f.cget("class") == "MyClass");
    CHECK(f.cget("container") == "1");
}

TEST_CASE("ttk::Separator: the constructor's options are actually applied (previously a bug silently ignored them)")
{
    tk::Tk root;
    root.withdraw();

    ttk::Separator sep(root, {{"orient", "vertical"}});
    CHECK(sep.cget("orient") == "vertical");
}

TEST_CASE("For an ordinary (non-read-only) option, specifying it at construction and via configure afterward give the same result")
{
    tk::Tk root;
    root.withdraw();

    tk::Label l1(root, {{"text", "hello"}});
    tk::Label l2(root);
    l2.config({{"text", "hello"}});

    CHECK(l1.cget("text") == l2.cget("text"));
}
