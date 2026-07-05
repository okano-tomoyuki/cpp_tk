// InterpreterClient::call()(エスケープハッチ)と、Widget/PhotoImage/font::Font/ttk::Style/Var
// 各クラスでのcall()公開・未初期化アクセス検知の回帰テスト。
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "cpp_tk.hpp"

namespace tk  = cpp_tk;
namespace ttk = tk::ttk;

TEST_CASE("call()は5クラス全てでpublicに呼び出せる(エスケープハッチ)")
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

    tk::PhotoImage img(root);
    ok = false;
    img.call({"image", "width", img.name()}, &ok);
    CHECK(ok);

    tk::font::Font font(root);
    ok = false;
    font.call({"font", "actual", font.name()}, &ok);
    CHECK(ok);

    ttk::Style style(root);
    ok = false;
    style.call({"ttk::style", "theme", "use"}, &ok);
    CHECK(ok);

    tk::StringVar sv(root);
    ok = false;
    sv.call({"set", sv.name(), "direct"}, &ok);
    CHECK(ok);
    CHECK(sv.get() == "direct");
}

TEST_CASE("未初期化オブジェクトへのcall()はErrorを送出し、戻り値はfalse/空になる")
{
    tk::set_error_policy(tk::ErrorPolicy::DEFAULT);

    tk::Button uninit_btn;
    bool ok = true;
    auto ret = uninit_btn.call({"winfo", "exists", "."}, &ok);
    CHECK_FALSE(ok);
    CHECK(ret.empty());

    tk::PhotoImage uninit_img;
    ok = true;
    uninit_img.call({"foo"}, &ok);
    CHECK_FALSE(ok);

    tk::font::Font uninit_font;
    ok = true;
    uninit_font.call({"foo"}, &ok);
    CHECK_FALSE(ok);

    ttk::Style uninit_style;
    ok = true;
    uninit_style.call({"foo"}, &ok);
    CHECK_FALSE(ok);

    // Var::get_var()はbool* success相当の引数を取らないため、DEFAULT(全カテゴリでError送出)下では
    // 未初期化アクセスも例外になる。ログのみで継続させたい場合はLENIENT_CALLを指定する。
    tk::StringVar uninit_var;
    CHECK_THROWS_AS(uninit_var.get_var(), const tk::Error&);

    tk::set_error_policy(tk::ErrorPolicy::LENIENT_CALL);
    CHECK(uninit_var.get_var().empty());
    tk::set_error_policy(tk::ErrorPolicy::DEFAULT);
}

TEST_CASE("ArgValue(Var&): configにVarを直接渡せる(name()を書かずに済む)")
{
    tk::set_error_policy(tk::ErrorPolicy::DEFAULT);
    tk::Tk root;
    root.withdraw();

    tk::BooleanVar bv(root);
    tk::Menu menu(root);
    CHECK_NOTHROW(menu.add_checkbutton({{"label", "check"}, {"variable", bv}}));

    tk::StringVar sv(root);
    tk::Entry entry(root);
    entry.config({{"textvariable", sv}});
    sv.set("via-config");
    CHECK(entry.get() == "via-config");
}

TEST_CASE("Entry::textvariable / Combobox::set/getがtext_var_無しで正しく動作する(回帰)")
{
    tk::set_error_policy(tk::ErrorPolicy::DEFAULT);
    tk::Tk root;
    root.withdraw();

    tk::StringVar sv(root);
    tk::Entry entry(root);
    entry.textvariable(sv);
    sv.set("hello");
    CHECK(entry.get() == "hello");
    entry.erase("0", "end");
    entry.insert("0", "world");
    CHECK(sv.get() == "world"); // ウィジェット側の変更がVarにも反映される(Tkの自動同期)

    ttk::Combobox combo(root);
    combo.values({"a", "b", "c"});
    combo.set("b");
    CHECK(combo.get() == "b");
}
