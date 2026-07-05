// docs/todo.md の「不具合」「構造的な設計課題」節の回帰テスト:
// - ttk::Treeview::bbox()がcolumn省略時にエラーにならないこと
// - Widget構築時にoptionsが生成コマンド自体へ埋め込まれ、生成時専用(read-only)オプションも
//   設定できるようになったこと(ttk::PanedWindowの-orient、Frameの-class/-container)
// - 副産物として見つかった既存バグ(ttk::Separatorがoptionsを無視していた)の回帰
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "cpp_tk.hpp"

namespace tk  = cpp_tk;
namespace ttk = tk::ttk;

TEST_CASE("ttk::Treeview::bbox: columnを省略してもエラーにならない")
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

TEST_CASE("ttk::PanedWindow: 生成時専用オプション-orientをコンストラクタ経由で設定できる")
{
    tk::Tk root;
    root.withdraw();

    ttk::PanedWindow pw(root, {{"orient", "horizontal"}});
    CHECK(pw.cget("orient") == "horizontal");
}

TEST_CASE("Frame: 生成時専用オプション-class/-containerをコンストラクタ経由で設定できる")
{
    tk::Tk root;
    root.withdraw();

    tk::Frame f(root, {{"class", "MyClass"}, {"container", true}});
    CHECK(f.cget("class") == "MyClass");
    CHECK(f.cget("container") == "1");
}

TEST_CASE("ttk::Separator: コンストラクタのoptionsが実際に適用される(過去は無視されるバグがあった)")
{
    tk::Tk root;
    root.withdraw();

    ttk::Separator sep(root, {{"orient", "vertical"}});
    CHECK(sep.cget("orient") == "vertical");
}

TEST_CASE("既存の通常オプション(read-only出ないもの)は構築時指定と構築後configureで同じ結果になる")
{
    tk::Tk root;
    root.withdraw();

    tk::Label l1(root, {{"text", "hello"}});
    tk::Label l2(root);
    l2.config({{"text", "hello"}});

    CHECK(l1.cget("text") == l2.cget("text"));
}
