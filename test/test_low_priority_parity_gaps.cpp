// Regression tests for categories B/C/D of the 5th inventory pass (see docs/tasks.md section A):
// PhotoImage/BitmapImage image manipulation, cpp_tk::custom::simpledialog, ttk::Style's
// advanced theming features, Text::bbox/xview/xscrollcommand, font::families/names & Font::copy,
// Canvas::postscript, Widget's grab_current/grab_status & winfo_id family & option_add/get &
// tk_focusNext/Prev, and Tk/Toplevel's iconname.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "cpp_tk.hpp"
#include "custom.hpp"

namespace tk     = cpp_tk;
namespace ttk    = tk::ttk;
namespace custom = tk::custom;

// ---- B: PhotoImage/BitmapImage image manipulation ----

TEST_CASE("PhotoImage: put/get/width/height/blank work")
{
    tk::Tk root;
    root.withdraw();

    tk::PhotoImage img;
    CHECK_NOTHROW(img.put("{red green} {blue red}"));
    CHECK(img.width() == 2);
    CHECK(img.height() == 2);

    auto pixel = img.get(0, 0);
    CHECK_FALSE(pixel.empty()); // an rgb string like "255 0 0"

    CHECK_NOTHROW(img.blank());
}

TEST_CASE("PhotoImage: copy/zoom/subsample return a new PhotoImage")
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

TEST_CASE("BitmapImage: width/height work")
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

// ---- B: ttk::Style advanced theming features ----

TEST_CASE("ttk::Style: layout()'s getter/setter work")
{
    tk::Tk root;
    root.withdraw();

    ttk::Style style;
    auto original_layout = style.layout("TButton");
    CHECK_FALSE(original_layout.empty());

    // Setting the retrieved layout back as-is should not throw (round-trip check).
    CHECK_NOTHROW(style.layout("TButton", original_layout));
}

TEST_CASE("ttk::Style: theme_create() can define a new theme")
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

// Regression tests for simpledialog live in test_simpledialog_return.cpp /
// test_simpledialog_escape.cpp instead (askstring() creates a modal dialog with grab_set(), and
// running it twice in a row within the same process was found to hang - see the addendum to the
// 5th inventory pass in docs/tasks.md).

// ---- C: Text::bbox/xview/xscrollcommand ----

TEST_CASE("Text: bbox/xview/xscrollcommand work")
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

TEST_CASE("font::families/names: return non-empty lists")
{
    tk::Tk root;
    root.withdraw();

    auto fam = tk::font::families();
    CHECK_FALSE(fam.empty());

    auto names = tk::font::names();
    CHECK_FALSE(names.empty());
}

TEST_CASE("Font::copy: makes a copy that carries over the configuration")
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

TEST_CASE("Canvas::postscript: returns PostScript data as a string")
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

// ---- D: grab_current/grab_status, winfo_id family, option_add/get, tk_focusNext/Prev, iconname ----

TEST_CASE("Widget: grab_current/grab_status work")
{
    tk::Tk root;
    // An earlier TEST_CASE may have already left the window mapped, in which case deiconify()
    // alone produces no state change (no Visibility event) and wait_visibility() would hang.
    // withdraw() first guarantees an actual unmapped state before deiconify() re-maps it, so a
    // change always occurs.
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

TEST_CASE("Widget: winfo_id/name/parent/depth/geometry work")
{
    tk::Tk root;
    // An earlier TEST_CASE may have already left the window mapped, in which case deiconify()
    // alone produces no state change (no Visibility event) and wait_visibility() would hang.
    // withdraw() first guarantees an actual unmapped state before deiconify() re-maps it, so a
    // change always occurs.
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

TEST_CASE("Widget: option_add/option_get work")
{
    tk::Tk root;
    root.withdraw();

    root.option_add("*Label.foreground", "#123456");
    tk::Label label(root);
    auto value = label.option_get("foreground", "Foreground");
    CHECK(value == "#123456");
}

TEST_CASE("Widget: tk_focusNext/tk_focusPrev work")
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

TEST_CASE("Tk/Toplevel: iconname's getter/setter work")
{
    tk::Tk root;
    root.withdraw();

    root.iconname("MyApp");
    CHECK(root.iconname() == "MyApp");

    tk::Toplevel top(root);
    top.iconname("SubWindow");
    CHECK(top.iconname() == "SubWindow");
}
