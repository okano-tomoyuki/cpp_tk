// Regression tests for docs/tasks.md section A, 3rd inventory pass (classes that were entirely
// unimplemented): BitmapImage / ttk::Menubutton / ttk::PanedWindow / ScrolledText / ttk::LabeledScale
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "cpp_tk.hpp"
#include "custom.hpp"

namespace tk     = cpp_tk;
namespace ttk    = tk::ttk;
namespace custom = tk::custom;

TEST_CASE("BitmapImage: can be created from inline XBM-format data")
{
    tk::Tk root;
    root.withdraw();

    // Use -data instead of -file to specify the XBM data inline, so this test has no external file dependency.
    std::string xbm =
        "#define im_width 8\n"
        "#define im_height 8\n"
        "static unsigned char im_bits[] = {\n"
        "  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};\n";
    tk::BitmapImage img({{"data", xbm}});
    CHECK_FALSE(img.name().empty());

    bool ok = false;
    auto type = img.call({"image", "type", img.name()}, &ok);
    CHECK(ok);
    CHECK(type == "bitmap");
}

TEST_CASE("ttk::Menubutton: menu() attaches a Menu")
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

    // ttk::panedwindow's -orient is a read-only option that can only be set at construction
    // time; since the Widget base constructor now embeds options directly into the creation
    // command (see the "structural design issue" fix in docs/tasks.md's 5th inventory pass),
    // it can be specified via the constructor's options map.
    ttk::PanedWindow pw(root, {{"orient", "horizontal"}});
    CHECK(pw.cget("orient") == "horizontal");
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

TEST_CASE("ScrolledText: Text+Scrollbar work together, and Text can be manipulated via text()")
{
    tk::Tk root;
    root.withdraw();

    custom::ScrolledText st(root);
    st.pack({{"fill", "both"}, {"expand", "true"}});

    st.text().insert("1.0", "line1\nline2\nline3\n");
    CHECK(st.text().get("1.0", "1.end") == "line1");

    // The yscrollcommand/command wiring works without throwing (internally reconstructs via handle()).
    CHECK_NOTHROW(st.text().yview("moveto 0.5"));
    CHECK_NOTHROW(st.scrollbar().set("0.2 0.8"));
}

TEST_CASE("ttk::LabeledScale: label()'s displayed text updates as the variable changes")
{
    tk::Tk root;
    root.withdraw();

    tk::DoubleVar var;
    custom::LabeledScale ls(root, var, 0, 100);
    ls.pack();

    CHECK(ls.label().cget("text") == "0");

    ls.scale().set(42);
    ls.update();
    CHECK(ls.label().cget("text") == "42");
}
