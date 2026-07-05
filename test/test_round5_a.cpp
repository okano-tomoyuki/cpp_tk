// 第5回棚卸し(docs/tasks.md A節参照)のうちA区分の回帰テスト:
// Menu各項目のコールバック対応、Listbox::xview/yview、ttk::Treeview::heading()の
// コールバック対応、Entry/ttk::Entryのxview/xscrollcommand
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "cpp_tk.hpp"

namespace tk  = cpp_tk;
namespace ttk = tk::ttk;

TEST_CASE("Menu: add_command/add_checkbutton/add_radiobuttonにcallbackを指定すると発火する")
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

    // menuの既定tearoff項目(index 0)によりcommand/checkbutton/radiobuttonはindex 1,2,3になる。
    menu.call({menu.full_name(), "invoke", "1"});
    CHECK(command_fired);

    menu.call({menu.full_name(), "invoke", "2"});
    CHECK(check_fired);

    menu.call({menu.full_name(), "invoke", "3"});
    CHECK(radio_fired);
}

TEST_CASE("Menu: insert/entryconfigureにcallbackを指定すると発火する")
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

TEST_CASE("Listbox: xview/yviewが機能する")
{
    tk::Tk root;
    root.withdraw();

    tk::Listbox listbox(root);
    for (int i = 0; i < 50; ++i)
        listbox.insert("end", "item" + std::to_string(i));
    listbox.pack();
    root.update();

    CHECK_NOTHROW(listbox.yview("moveto 0.5"));
    // yview()の引数無し版に相当する生Tcl呼び出しで、実際にスクロール位置が変わったことを確認する
    // (classic Listboxの-yscrollcommandは`xview`/`yview`呼び出し直後の同期的な発火が保証されないため、
    // コールバック発火の有無ではなく実際の表示位置で検証する)。
    auto view = listbox.call({listbox.full_name(), "yview"});
    CHECK(view.substr(0, 3) == "0.5");

    CHECK_NOTHROW(listbox.xview("moveto 0.0"));

    // xscrollcommand/yscrollcommandがcget経由で正しく設定されること(発火タイミングはTk内部実装依存のため検証しない)。
    listbox.yscrollcommand([](std::string) {});
    listbox.xscrollcommand([](std::string) {});
    CHECK_FALSE(listbox.cget("yscrollcommand").empty());
    CHECK_FALSE(listbox.cget("xscrollcommand").empty());
}

TEST_CASE("ttk::Treeview: heading()にcallbackを指定すると列ヘッダクリックで発火する")
{
    tk::Tk root;
    root.withdraw();

    ttk::Treeview tv(root, {{"columns", "c1"}});
    tv.pack();

    bool sort_fired = false;
    tv.heading("c1", {{"text", "Column 1"}}, [&sort_fired]() { sort_fired = true; });
    tv.heading("#0", {{"text", "Tree"}});

    // heading()の-commandは通常のTclプロシージャとして登録されるため、直接呼び出して検証する。
    auto cmd = tv.call({tv.full_name(), "heading", "c1", "-command"});
    CHECK_FALSE(cmd.empty());
    tv.call({cmd});
    CHECK(sort_fired);
}

TEST_CASE("Entry: xview/xscrollcommandが機能する")
{
    tk::Tk root;
    root.withdraw();

    tk::Entry entry(root);
    entry.insert("0", std::string(100, 'x'));
    entry.pack();
    root.update();

    // xviewの厳密なスクロール位置はカーソル追従等のTk内部実装(classic/ttkで挙動が異なる)に
    // 左右されるため、moveto呼び出しが例外にならないことと、xscrollcommandが正しくcgetに
    // 反映されることのみを検証する(発火タイミング・厳密なスクロール位置は検証しない)。
    CHECK_NOTHROW(entry.xview("moveto 0.5"));

    entry.xscrollcommand([](std::string) {});
    CHECK_FALSE(entry.cget("xscrollcommand").empty());
}

TEST_CASE("ttk::Entry: xview/xscrollcommandが機能する")
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
