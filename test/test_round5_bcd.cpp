// 第5回棚卸し(docs/tasks.md A節参照)のうちB/C/D区分の回帰テスト:
// PhotoImage/BitmapImageの画像操作系、cpp_tk::custom::simpledialog、ttk::Styleの高度なテーマ機能、
// Text::bbox/xview/xscrollcommand、font::families/names・Font::copy、Canvas::postscript、
// Widgetのgrab_current/grab_status・winfo_id系・option_add/get・tk_focusNext/Prev、Tk/Toplevelのiconname
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "cpp_tk.hpp"
#include "custom.hpp"

namespace tk     = cpp_tk;
namespace ttk    = tk::ttk;
namespace custom = tk::custom;

// ---- B: PhotoImage/BitmapImage 画像操作系 ----

TEST_CASE("PhotoImage: put/get/width/height/blankが機能する")
{
    tk::Tk root;
    root.withdraw();

    tk::PhotoImage img;
    CHECK_NOTHROW(img.put("{red green} {blue red}"));
    CHECK(img.width() == 2);
    CHECK(img.height() == 2);

    auto pixel = img.get(0, 0);
    CHECK_FALSE(pixel.empty()); // "255 0 0"のようなrgb文字列

    CHECK_NOTHROW(img.blank());
}

TEST_CASE("PhotoImage: copy/zoom/subsampleが新規PhotoImageを返す")
{
    tk::Tk root;
    root.withdraw();

    tk::PhotoImage img;
    img.put("{red green blue red} {blue red green blue} {red green blue red} {blue red green blue}");
    REQUIRE(img.width() == 4);
    REQUIRE(img.height() == 4);

    auto copied = img.copy();
    CHECK(copied.name() != img.name());
    CHECK(copied.width() == 4);
    CHECK(copied.height() == 4);

    auto zoomed = img.zoom(2);
    CHECK(zoomed.width() == 8);
    CHECK(zoomed.height() == 8);

    auto subsampled = img.subsample(2);
    CHECK(subsampled.width() == 2);
    CHECK(subsampled.height() == 2);

    tk::PhotoImage dest;
    CHECK_NOTHROW(dest.copy_from(img));
    CHECK(dest.width() == 4);
}

TEST_CASE("BitmapImage: width/heightが機能する")
{
    tk::Tk root;
    root.withdraw();

    std::string xbm =
        "#define im_width 8\n"
        "#define im_height 8\n"
        "static unsigned char im_bits[] = {\n"
        "  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};\n";
    tk::BitmapImage img({{"data", xbm}});

    CHECK(img.width() == 8);
    CHECK(img.height() == 8);
}

// ---- B: ttk::Style 高度なテーマ機能 ----

TEST_CASE("ttk::Style: layout()のgetter/setterが機能する")
{
    tk::Tk root;
    root.withdraw();

    ttk::Style style;
    auto original_layout = style.layout("TButton");
    CHECK_FALSE(original_layout.empty());

    // 取得したレイアウトをそのまま設定し直しても例外にならないこと(往復確認)。
    CHECK_NOTHROW(style.layout("TButton", original_layout));
}

TEST_CASE("ttk::Style: theme_create()で新規テーマを定義できる")
{
    tk::Tk root;
    root.withdraw();

    ttk::Style style;
    auto before = style.theme_names();

    style.theme_create("cpp_tk_test_theme", "default");

    auto after = style.theme_names();
    bool found = false;
    for (auto& t : after) if (t == "cpp_tk_test_theme") found = true;
    CHECK(found);
}

// simpledialogの回帰テストはtest_simpledialog_return.cpp/test_simpledialog_escape.cppに分離している
// (askstring()はgrab_set()を伴うモーダルダイアログを生成するため、同一プロセス内で2回連続実行すると
// ハングする現象が判明したため、docs/tasks.md 第5回棚卸しの追記を参照)。

// ---- C: Text::bbox/xview/xscrollcommand ----

TEST_CASE("Text: bbox/xview/xscrollcommandが機能する")
{
    tk::Tk root;
    root.withdraw();

    tk::Text text(root, {{"wrap", "none"}});
    text.pack();
    text.insert("1.0", "hello world\n");
    root.update();

    auto box = text.bbox("1.0");
    CHECK(box.size() == 4);

    CHECK_NOTHROW(text.xview("moveto 0.0"));

    bool called = false;
    text.xscrollcommand([&called](std::string) { called = true; });
    text.xview("moveto 0.1");
    root.update();
    CHECK(called);
}

// ---- C: font::families()/names(), Font::copy() ----

TEST_CASE("font::families/names: 空でない一覧を返す")
{
    tk::Tk root;
    root.withdraw();

    auto fam = tk::font::families();
    CHECK_FALSE(fam.empty());

    auto names = tk::font::names();
    CHECK_FALSE(names.empty());
}

TEST_CASE("Font::copy: 設定を引き継いだ複製を作る")
{
    tk::Tk root;
    root.withdraw();

    tk::font::Font original({{"family", "Courier"}, {"size", 14}});
    auto copied = original.copy();

    CHECK(copied.name() != original.name());
    CHECK(copied.actual("size") == original.actual("size"));
    CHECK(copied.actual("family") == original.actual("family"));
}

// ---- C: Canvas::postscript ----

TEST_CASE("Canvas::postscript: PostScriptデータを文字列で返す")
{
    tk::Tk root;
    root.withdraw();

    tk::Canvas canvas(root, {{"width", 100}, {"height", 100}});
    canvas.pack();
    canvas.create_rectangle(10, 10, 50, 50);
    root.update();

    auto ps = canvas.postscript();
    CHECK(ps.find("%!PS") != std::string::npos);
}

// ---- D: grab_current/grab_status, winfo_id系, option_add/get, tk_focusNext/Prev, iconname ----

TEST_CASE("Widget: grab_current/grab_statusが機能する")
{
    tk::Tk root;
    // 先行するTEST_CASEが既にmapped状態にしている可能性があり、その場合deiconify()単体では
    // 状態変化(Visibilityイベント)が発生せずwait_visibility()がハングする。withdraw()で
    // 一旦確実にunmapしてからdeiconify()することで、必ず変化を起こす。
    root.withdraw();
    root.deiconify();
    root.geometry("50x50-3000-3000");
    root.wait_visibility();

    CHECK(root.grab_status() == "none");
    root.grab_set();
    CHECK(root.grab_status() != "none");
    CHECK(root.grab_current() == root.full_name());
    root.grab_release();
    CHECK(root.grab_status() == "none");
}

TEST_CASE("Widget: winfo_id/name/parent/depth/geometryが機能する")
{
    tk::Tk root;
    // 先行するTEST_CASEが既にmapped状態にしている可能性があり、その場合deiconify()単体では
    // 状態変化(Visibilityイベント)が発生せずwait_visibility()がハングする。withdraw()で
    // 一旦確実にunmapしてからdeiconify()することで、必ず変化を起こす。
    root.withdraw();
    root.deiconify();
    root.geometry("50x50-3000-3000");
    root.wait_visibility();

    tk::Frame child(root);
    child.pack();
    root.update();

    CHECK_FALSE(child.winfo_id().empty());
    CHECK_FALSE(child.winfo_name().empty());
    CHECK(child.winfo_parent() == root.full_name());
    CHECK(child.winfo_depth() > 0);
    CHECK_FALSE(root.winfo_geometry().empty());
}

TEST_CASE("Widget: option_add/option_getが機能する")
{
    tk::Tk root;
    root.withdraw();

    root.option_add("*Label.foreground", "#123456");
    tk::Label label(root);
    auto value = label.option_get("foreground", "Foreground");
    CHECK(value == "#123456");
}

TEST_CASE("Widget: tk_focusNext/tk_focusPrevが機能する")
{
    tk::Tk root;
    root.geometry("50x50-3000-3000");

    tk::Entry e1(root);
    tk::Entry e2(root);
    e1.pack();
    e2.pack();
    root.update();

    auto next = e1.tk_focusNext();
    CHECK_FALSE(next.full_name().empty());

    auto prev = next.tk_focusPrev();
    CHECK(prev.full_name() == e1.full_name());
}

TEST_CASE("Tk/Toplevel: iconnameのgetter/setterが機能する")
{
    tk::Tk root;
    root.withdraw();

    root.iconname("MyApp");
    CHECK(root.iconname() == "MyApp");

    tk::Toplevel top(root);
    top.iconname("SubWindow");
    CHECK(top.iconname() == "SubWindow");
}
