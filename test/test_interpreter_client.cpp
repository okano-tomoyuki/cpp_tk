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

// 「未初期化オブジェクトへのcall()」の回帰テストはtest_uninitialized_objects.cppに分離した
// (PhotoImage/font::Font/ttk::Style/Varはparentを取らずcurrent_interp()に暗黙で束縛するように
// なったため、このファイルのように他のTEST_CASEで既にtk::Tkを構築済みのプロセス内では
// current_interp()が非nullptrになってしまい、意図した「未初期化」状態を検証できない。
// docs/tasks.md 参照)。

TEST_CASE("ArgValue(Var&): configにVarを直接渡せる(name()を書かずに済む)")
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

TEST_CASE("Entry::textvariable / Combobox::set/getがtext_var_無しで正しく動作する(回帰)")
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
    CHECK(sv.get() == "world"); // ウィジェット側の変更がVarにも反映される(Tkの自動同期)

    ttk::Combobox combo(root);
    combo.values({"a", "b", "c"});
    combo.set("b");
    CHECK(combo.get() == "b");
}
