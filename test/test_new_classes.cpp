// docs/tasks.md A節 第3回棚卸し(クラスそのものが未実装だったもの)の回帰テスト:
// BitmapImage / ttk::Menubutton / ttk::PanedWindow / ScrolledText / ttk::LabeledScale
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "cpp_tk.hpp"
#include "custom.hpp"

namespace tk     = cpp_tk;
namespace ttk    = tk::ttk;
namespace custom = tk::custom;

TEST_CASE("BitmapImage: XBM形式のインラインデータから生成できる")
{
    tk::Tk root;
    root.withdraw();

    // 外部ファイルに依存しないよう、-fileではなく-dataでXBM形式データをインライン指定する。
    std::string xbm =
        "#define im_width 8\n"
        "#define im_height 8\n"
        "static unsigned char im_bits[] = {\n"
        "  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};\n";
    tk::BitmapImage img(root, {{"data", xbm}});
    CHECK_FALSE(img.name().empty());

    bool ok = false;
    auto type = img.call({"image", "type", img.name()}, &ok);
    CHECK(ok);
    CHECK(type == "bitmap");
}

TEST_CASE("ttk::Menubutton: menu()でMenuを紐づけられる")
{
    tk::Tk root;
    root.withdraw();

    tk::Menu menu(root);
    menu.add_command({{"label", "item1"}});

    ttk::Menubutton mb(root);
    mb.menu(&menu);
    mb.pack();

    CHECK(mb.cget("menu") == menu.full_name());
}

TEST_CASE("ttk::PanedWindow: add/forget/panes/sashpos")
{
    tk::Tk root;
    root.withdraw();

    // ttk::panedwindowの-orientは生成時にしか指定できない読み取り専用オプションだが、
    // cpp_tkのWidget構築は常に「生成」→「configureで別途設定」の2段階のため、現状は
    // 生成時専用オプションを設定する手段が無い(docs/tasks.md 発見事項として別途記録)。
    // ここではorient指定を諦め既定(vertical)のままadd/forget/panes/sashposのみ検証する。
    ttk::PanedWindow pw(root);
    pw.pack({{"fill", "both"}, {"expand", "true"}});

    tk::Frame pane1(pw);
    tk::Frame pane2(pw);
    pw.add(pane1);
    pw.add(pane2);
    pw.update();

    auto panes = pw.panes();
    REQUIRE(panes.size() == 2);
    CHECK(panes[0] == pane1.full_name());

    CHECK_NOTHROW(pw.sashpos(0));
    CHECK_NOTHROW(pw.sashpos(0, 50));

    pw.forget(pane2);
    CHECK(pw.panes().size() == 1);
}

TEST_CASE("ScrolledText: Text+Scrollbarが連携し、text()経由でText操作ができる")
{
    tk::Tk root;
    root.withdraw();

    custom::ScrolledText st(root);
    st.pack({{"fill", "both"}, {"expand", "true"}});

    st.text().insert("1.0", "line1\nline2\nline3\n");
    CHECK(st.text().get("1.0", "1.end") == "line1");

    // yscrollcommand/command経由の連携が例外なく機能すること(内部でhandle()経由の再構築を使う)
    CHECK_NOTHROW(st.text().yview("moveto 0.5"));
    CHECK_NOTHROW(st.scrollbar().set("0.2 0.8"));
}

TEST_CASE("ttk::LabeledScale: 変数の変化に応じてlabel()の表示が更新される")
{
    tk::Tk root;
    root.withdraw();

    tk::DoubleVar var(root);
    custom::LabeledScale ls(root, var, 0, 100);
    ls.pack();

    CHECK(ls.label().cget("text") == "0");

    ls.scale().set(42);
    ls.update();
    CHECK(ls.label().cget("text") == "42");
}
