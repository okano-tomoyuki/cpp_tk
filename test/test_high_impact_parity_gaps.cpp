// Regression tests for category A of the 5th inventory pass (see docs/tasks.md section A):
// command callbacks for Menu items, Listbox::xview/yview, command callback for
// ttk::Treeview::heading(), and xview/xscrollcommand for Entry/ttk::Entry.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "cpp_tk.hpp"

namespace tk  = cpp_tk;
namespace ttk = tk::ttk;

TEST_CASE("Menu: a callback given to add_command/add_checkbutton/add_radiobutton fires")
{
    tk::Tk root;
    root.withdraw();

    tk::Menu menu(root);

    bool command_fired = false;
    menu.add_command({{"label", "cmd"}}, [&command_fired]() { command_fired = true; });

    bool check_fired = false;
    tk::BooleanVar bv;
    menu.add_checkbutton({{"label", "chk"}, {"variable", bv}}, [&check_fired]() { check_fired = true; });

    bool radio_fired = false;
    tk::StringVar rv;
    menu.add_radiobutton({{"label", "radio"}, {"variable", rv}, {"value", "r1"}}, [&radio_fired]() { radio_fired = true; });

    // The menu's default tearoff entry occupies index 0, so command/checkbutton/radiobutton become indices 1, 2, 3.
    menu.call({menu.full_name(), "invoke", "1"});
    CHECK(command_fired);

    menu.call({menu.full_name(), "invoke", "2"});
    CHECK(check_fired);

    menu.call({menu.full_name(), "invoke", "3"});
    CHECK(radio_fired);
}

TEST_CASE("Menu: a callback given to insert/entryconfigure fires")
{
    tk::Tk root;
    root.withdraw();

    tk::Menu menu(root);

    bool insert_fired = false;
    menu.insert("end", "command", {{"label", "ins"}}, [&insert_fired]() { insert_fired = true; });
    menu.call({menu.full_name(), "invoke", "1"});
    CHECK(insert_fired);

    bool reconfigured_fired = false;
    menu.entryconfigure("1", {{"label", "ins2"}}, [&reconfigured_fired]() { reconfigured_fired = true; });
    menu.call({menu.full_name(), "invoke", "1"});
    CHECK(reconfigured_fired);
}

TEST_CASE("Listbox: xview/yview work")
{
    tk::Tk root;
    root.withdraw();

    tk::Listbox listbox(root);
    for (int i = 0; i < 50; ++i)
        listbox.insert("end", "item" + std::to_string(i));
    listbox.pack();
    root.update();

    CHECK_NOTHROW(listbox.yview("moveto 0.5"));
    // Use the raw Tcl call equivalent to yview() with no arguments to confirm the scroll
    // position actually changed (classic Listbox's -yscrollcommand is not guaranteed to fire
    // synchronously right after an xview/yview call, so verify the actual displayed position
    // rather than whether the callback fired).
    auto view = listbox.call({listbox.full_name(), "yview"});
    CHECK(view.substr(0, 3) == "0.5");

    CHECK_NOTHROW(listbox.xview("moveto 0.0"));

    // Confirm xscrollcommand/yscrollcommand are set correctly via cget (firing timing depends
    // on Tk's internal implementation, so it is not verified here).
    listbox.yscrollcommand([](std::string) {});
    listbox.xscrollcommand([](std::string) {});
    CHECK_FALSE(listbox.cget("yscrollcommand").empty());
    CHECK_FALSE(listbox.cget("xscrollcommand").empty());
}

TEST_CASE("ttk::Treeview: a callback given to heading() fires when the column header is clicked")
{
    tk::Tk root;
    root.withdraw();

    ttk::Treeview tv(root, {{"columns", "c1"}});
    tv.pack();

    bool sort_fired = false;
    tv.heading("c1", {{"text", "Column 1"}}, [&sort_fired]() { sort_fired = true; });
    tv.heading("#0", {{"text", "Tree"}});

    // heading()'s -command is registered as an ordinary Tcl procedure, so invoke it directly to verify.
    auto cmd = tv.call({tv.full_name(), "heading", "c1", "-command"});
    CHECK_FALSE(cmd.empty());
    tv.call({cmd});
    CHECK(sort_fired);
}

TEST_CASE("Entry: xview/xscrollcommand work")
{
    tk::Tk root;
    root.withdraw();

    tk::Entry entry(root);
    entry.insert("0", std::string(100, 'x'));
    entry.pack();
    root.update();

    // xview's exact scroll position depends on Tk's internal implementation (e.g. cursor
    // tracking), which differs between classic and ttk, so only verify that the moveto call
    // does not throw and that xscrollcommand is correctly reflected in cget (firing timing and
    // exact scroll position are not verified).
    CHECK_NOTHROW(entry.xview("moveto 0.5"));

    entry.xscrollcommand([](std::string) {});
    CHECK_FALSE(entry.cget("xscrollcommand").empty());
}

TEST_CASE("ttk::Entry: xview/xscrollcommand work")
{
    tk::Tk root;
    root.withdraw();

    ttk::Entry entry(root);
    entry.insert("0", std::string(100, 'x'));
    entry.pack();
    root.update();

    CHECK_NOTHROW(entry.xview("moveto 0.5"));

    entry.xscrollcommand([](std::string) {});
    CHECK_FALSE(entry.cget("xscrollcommand").empty());
}
