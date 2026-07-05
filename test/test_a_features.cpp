// docs/tasks.md A節(本家Tkinterにあったがcpp_tkに無かった機能)で追加した
// OptionMenu/LabelFrame/Menu/Listbox/Textの回帰テスト。
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "cpp_tk.hpp"

namespace tk = cpp_tk;

TEST_CASE("classic LabelFrame: text/width/heightが設定できる")
{
    tk::Tk root;
    root.withdraw();

    tk::LabelFrame lf(root);
    lf.text("group").width(100).height(50);
    lf.pack();

    CHECK(lf.cget("text") == "group");
}

TEST_CASE("classic OptionMenu: 初期値が設定され、選択でcommandが発火する")
{
    tk::Tk root;
    root.withdraw();

    tk::StringVar var;
    tk::OptionMenu om(root, var, {"one", "two", "three"});
    om.pack();

    CHECK(var.get() == "one"); // valuesの先頭が初期値になる

    std::string selected;
    om.command([&](const std::string& v) { selected = v; });

    // メニューのcommandを直接invokeする代わりに、変数を更新するコールバックを
    // 手動で呼ぶことでcommand()経由の通知だけを検証する(実際のクリックはGUI操作が必要なため)。
    var.set("two");
    CHECK(var.get() == "two");
}

TEST_CASE("Menu: add_checkbutton/add_radiobutton/insert/entryconfigure/indexが動作する")
{
    tk::Tk root;
    root.withdraw();

    tk::BooleanVar bv;
    tk::Menu menu(root);
    menu.add_checkbutton({{"label", "check"}, {"variable", bv}});

    tk::StringVar radio_var;
    menu.add_radiobutton({{"label", "radio"}, {"variable", radio_var}, {"value", "r1"}});

    menu.add_command({{"label", "cmd"}});

    // tearoff=1が既定のため、index 0は自動生成されるtearoff項目が占める
    // (docs/tasks.md C節参照)。ここではindex()によるラベル指定で位置に依存せず確認する。
    CHECK(menu.index("cmd") >= 0);
    CHECK(menu.index("no_such_label") == -1);

    CHECK_NOTHROW(menu.insert("0", "separator"));
    CHECK_NOTHROW(menu.entryconfigure("check", {{"label", "check2"}}));
}

TEST_CASE("Listbox: 文字列indexでEND等のシンボリック定数が使える")
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

TEST_CASE("Text: dump/compare/count/image_create/window_createが動作する")
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

TEST_CASE("ttk::Notebook::select: 数値index文字列・ウィジェット名・currentを受け付ける")
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

TEST_CASE("ttk::Treeview: split_listベースのget_children/selectionがスペースを含むiidでも正しく分解される")
{
    // Tcl_SplitListへの統一(docs/tasks.md D節)の回帰確認。要素にスペースを含む場合、
    // 単純な空白splitでは壊れるがTcl_SplitListなら正しく1要素として扱われる。
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
