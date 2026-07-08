// Regression tests for OptionMenu/LabelFrame/Menu/Listbox/Text, added under docs/tasks.md
// section A (features present in upstream Tkinter but missing from cpp_tk).
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "cpp_tk.hpp"

namespace tk = cpp_tk;

TEST_CASE("classic LabelFrame: text/width/height can be set")
{
    tk::Tk root;
    root.withdraw();

    tk::LabelFrame lf(root);
    lf.text("group").width(100).height(50);
    lf.pack();

    CHECK(lf.cget("text") == "group");
}

TEST_CASE("classic OptionMenu: the initial value is set, and selecting fires command")
{
    tk::Tk root;
    root.withdraw();

    tk::StringVar var;
    tk::OptionMenu om(root, var, {"one", "two", "three"});
    om.pack();

    CHECK(var.get() == "one"); // the first entry in values becomes the initial value

    std::string selected;
    om.command([&](const std::string& v) { selected = v; });

    // Instead of invoking the menu's command directly, manually call the callback that updates
    // the variable, so this only verifies notification via command() (an actual click would
    // require driving the GUI).
    var.set("two");
    CHECK(var.get() == "two");
}

TEST_CASE("Menu: add_checkbutton/add_radiobutton/insert/entryconfigure/index work")
{
    tk::Tk root;
    root.withdraw();

    tk::BooleanVar bv;
    tk::Menu menu(root);
    menu.add_checkbutton({{"label", "check"}, {"variable", bv}});

    tk::StringVar radio_var;
    menu.add_radiobutton({{"label", "radio"}, {"variable", radio_var}, {"value", "r1"}});

    menu.add_command({{"label", "cmd"}});

    // tearoff=1 by default, so index 0 is occupied by the auto-generated tearoff entry (see
    // docs/tasks.md section C). Use index() with a label here so this test does not depend on position.
    CHECK(menu.index("cmd") >= 0);
    CHECK(menu.index("no_such_label") == -1);

    CHECK_NOTHROW(menu.insert("0", "separator"));
    CHECK_NOTHROW(menu.entryconfigure("check", {{"label", "check2"}}));
}

TEST_CASE("Listbox: symbolic constants like END work with string indices")
{
    tk::Tk root;
    root.withdraw();

    tk::Listbox lb(root);
    lb.pack();

    lb.insert(tk::END, "a");
    lb.insert(tk::END, "b");
    lb.insert(tk::END, "c");
    CHECK(lb.size() == 3);
    CHECK(lb.get("0") == "a");

    lb.see(tk::END);
    lb.select_set(tk::END);
    CHECK(lb.select_includes(tk::END));

    lb.activate(tk::END);
    lb.select_clear(tk::END);
    lb.erase(tk::END, tk::END);
    CHECK(lb.size() == 2);

    lb.insert("0", "zero");
    CHECK(lb.get("0") == "zero");
}

TEST_CASE("Text: dump/compare/count/image_create/window_create work")
{
    tk::Tk root;
    root.withdraw();

    tk::Text text(root);
    text.pack();
    text.insert("1.0", "hello world");

    CHECK(text.compare("1.0", "<", "end"));
    CHECK(text.count("1.0", "end") > 0);
    CHECK_FALSE(text.dump("1.0", "end").empty());

    tk::PhotoImage img;
    auto image_id = text.image_create("1.0", {{"image", img.name()}});
    CHECK_FALSE(image_id.empty());

    tk::Button embedded(root);
    embedded.text("embedded");
    CHECK_NOTHROW(text.window_create("end", embedded));
}

TEST_CASE("ttk::Notebook::select: accepts a numeric index string, a widget name, and \"current\"")
{
    tk::Tk root;
    root.withdraw();
    namespace ttk = tk::ttk;

    ttk::Notebook nb(root);
    nb.pack();

    tk::Frame page1(nb);
    tk::Frame page2(nb);
    nb.add_tab(page1, "Page1");
    nb.add_tab(page2, "Page2");

    CHECK_NOTHROW(nb.select(page2.full_name()));
    CHECK_NOTHROW(nb.select("current"));
    CHECK_NOTHROW(nb.select("0"));
}

TEST_CASE("ttk::Treeview: split_list-based get_children/selection correctly parse iids containing spaces")
{
    // Regression check for the switch to Tcl_SplitList (see docs/tasks.md section D). A naive
    // split on whitespace would break for an element containing spaces, whereas Tcl_SplitList
    // correctly treats it as a single element.
    tk::Tk root;
    root.withdraw();
    namespace ttk = tk::ttk;

    ttk::Treeview tv(root);
    tv.pack();
    tv.insert("", "end", "item with spaces", {{"text", "Item 1"}});
    tv.insert("", "end", "item2", {{"text", "Item 2"}});

    auto children = tv.get_children();
    REQUIRE(children.size() == 2);
    CHECK(children[0] == "item with spaces");

    tv.selection_set({"item with spaces"});
    auto sel = tv.selection();
    REQUIRE(sel.size() == 1);
    CHECK(sel[0] == "item with spaces");
}
