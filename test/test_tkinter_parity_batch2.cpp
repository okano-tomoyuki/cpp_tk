// Regression tests for docs/tasks.md section A, 2nd inventory pass (Widget common
// functionality/Canvas/Text/Entry/Scale/Spinbox/ttk::Notebook/ttk::Treeview/Toplevel & Tk).
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "cpp_tk.hpp"

namespace tk  = cpp_tk;
namespace ttk = tk::ttk;

TEST_CASE("Widget: bind_all/unbind_all/bind_class deliver events")
{
    tk::Tk root;
    // event_generate does not deliver while withdrawn, so keep the window mapped by placing it off-screen.
    root.geometry("1x1-3000-3000");
    tk::Frame f(root);
    f.pack();
    f.update();

    int all_fired = 0;
    root.bind_all("<<AllEvent>>", [&](const tk::Event&) { ++all_fired; });
    f.event_generate("<<AllEvent>>");
    f.update();
    CHECK(all_fired == 1);

    root.unbind_all("<<AllEvent>>");
    f.event_generate("<<AllEvent>>");
    f.update();
    CHECK(all_fired == 1);

    int class_fired = 0;
    root.bind_class("Frame", "<<ClassEvent>>", [&](const tk::Event&) { ++class_fired; });
    f.event_generate("<<ClassEvent>>");
    f.update();
    CHECK(class_fired == 1);
}

TEST_CASE("Widget: grid_slaves/pack_slaves/place_slaves + grid_info/pack_info/place_info")
{
    tk::Tk root;
    root.withdraw();

    // Tk does not allow mixing pack/grid among direct children of the same parent (place is
    // independent and can coexist with either), so use separate parent frames for grid and pack.
    tk::Frame grid_parent(root);
    tk::Frame pack_parent(root);
    tk::Label gridded(grid_parent);
    tk::Label packed(pack_parent);
    tk::Label placed(pack_parent);
    gridded.grid({{"row", 0}, {"column", 0}});
    packed.pack();
    placed.place({{"x", 5}, {"y", 5}});

    auto gslaves = grid_parent.grid_slaves();
    REQUIRE(gslaves.size() == 1);
    CHECK(gslaves[0] == gridded.full_name());

    auto pslaves = pack_parent.pack_slaves();
    REQUIRE(pslaves.size() == 1);
    CHECK(pslaves[0] == packed.full_name());

    auto plslaves = pack_parent.place_slaves();
    REQUIRE(plslaves.size() == 1);
    CHECK(plslaves[0] == placed.full_name());

    auto ginfo = gridded.grid_info();
    CHECK(ginfo.at("row") == "0");
    CHECK(ginfo.at("column") == "0");

    auto pinfo = placed.place_info();
    CHECK(pinfo.at("x") == "5");
    CHECK(pinfo.at("y") == "5");

    CHECK(packed.grid_info().empty()); // managed by pack, not grid
}

TEST_CASE("Widget: nametowidget/winfo_reqwidth/winfo_reqheight")
{
    tk::Tk root;
    root.withdraw();

    tk::Button btn(root);
    btn.text("hello");
    btn.pack();
    btn.update();

    CHECK(btn.winfo_reqwidth() > 0);
    CHECK(btn.winfo_reqheight() > 0);

    auto resolved = root.nametowidget(btn.full_name());
    CHECK(resolved.full_name() == btn.full_name());
    CHECK(resolved.cget("text") == "hello"); // usable as a generic Widget handle
}

TEST_CASE("Canvas: tag_raise/tag_lower/find_all/find_withtag/canvasx/canvasy/create_bitmap")
{
    tk::Tk root;
    root.withdraw();

    tk::Canvas canvas(root, {{"width", 200}, {"height", 200}});
    canvas.pack();

    auto r1 = canvas.create_rectangle(0, 0, 50, 50, {{"tags", "r1"}});
    auto r2 = canvas.create_rectangle(10, 10, 60, 60, {{"tags", "r2"}});
    CHECK_NOTHROW(canvas.tag_raise("r1"));
    CHECK_NOTHROW(canvas.tag_lower("r2"));

    auto all = canvas.find_all();
    CHECK(all.size() == 2);

    auto withtag = canvas.find_withtag("r1");
    REQUIRE(withtag.size() == 1);
    CHECK(withtag[0] == r1);

    CHECK_NOTHROW(canvas.canvasx(50));
    CHECK_NOTHROW(canvas.canvasy(50));

    CHECK_NOTHROW(canvas.create_bitmap(100, 100, {{"bitmap", "info"}}));
    (void)r2;
}

TEST_CASE("Text: index/tag_names/tag_ranges/tag_lower/tag_raise/tag_delete/edit_undo/edit_redo/edit_modified")
{
    tk::Tk root;
    root.withdraw();

    tk::Text text(root, {{"undo", true}});
    text.pack();
    text.insert("1.0", "hello world");

    CHECK(text.index("1.5") == "1.5");
    CHECK(text.index("end") != "");

    text.tag_add("greeting", "1.0", "1.5");
    auto names = text.tag_names();
    bool has_greeting = false;
    for (auto& n : names) if (n == "greeting") has_greeting = true;
    CHECK(has_greeting);

    auto ranges = text.tag_ranges("greeting");
    REQUIRE(ranges.size() == 2);
    CHECK(ranges[0] == "1.0");

    CHECK_NOTHROW(text.tag_raise("greeting"));
    CHECK_NOTHROW(text.tag_lower("greeting"));
    CHECK_NOTHROW(text.tag_delete("greeting"));

    // The initial insert("1.0", ...) is itself an edit operation, so reset the modified flag to false first.
    text.edit_modified(false);
    CHECK_FALSE(text.edit_modified());
    text.insert("end", "!");
    CHECK(text.edit_modified());
    text.edit_modified(false);
    CHECK_FALSE(text.edit_modified());

    CHECK_NOTHROW(text.edit_undo());
    CHECK_NOTHROW(text.edit_redo());
}

TEST_CASE("Entry: select_range/selection_clear/select_present")
{
    tk::Tk root;
    root.withdraw();

    tk::Entry entry(root);
    entry.pack();
    entry.insert("0", "hello world");

    CHECK_FALSE(entry.select_present());
    entry.select_range("0", "5");
    CHECK(entry.select_present());
    entry.selection_clear();
    CHECK_FALSE(entry.select_present());
}

TEST_CASE("Scale (classic/ttk): get/set")
{
    tk::Tk root;
    root.withdraw();

    tk::Scale scale(root);
    scale.from(0).to(100);
    scale.set(42);
    CHECK(scale.get() == doctest::Approx(42.0));

    ttk::Scale tscale(root);
    tscale.from(0).to(100);
    tscale.set(24);
    CHECK(tscale.get() == doctest::Approx(24.0));
}

TEST_CASE("Spinbox (classic/ttk): get()")
{
    tk::Tk root;
    root.withdraw();

    tk::Spinbox spin(root);
    spin.from(0).to(10);
    spin.pack();
    CHECK_NOTHROW(spin.get());

    ttk::Spinbox tspin(root);
    tspin.from(0).to(10);
    tspin.pack();
    CHECK_NOTHROW(tspin.get());
}

TEST_CASE("ttk::Notebook: tab/tabs/forget/hide/index/select getter")
{
    tk::Tk root;
    root.withdraw();

    ttk::Notebook nb(root);
    nb.pack();

    tk::Frame page1(nb);
    tk::Frame page2(nb);
    nb.add_tab(page1, "Page1");
    nb.add_tab(page2, "Page2");

    auto tabs = nb.tabs();
    REQUIRE(tabs.size() == 2);
    CHECK(tabs[0] == page1.full_name());

    CHECK(nb.index(page2.full_name()) == 1);

    nb.tab(page1.full_name(), {{"text", "Renamed"}});
    CHECK(nb.tab(page1.full_name(), "text") == "Renamed");

    nb.select(page2.full_name());
    CHECK(nb.select() == page2.full_name());

    CHECK_NOTHROW(nb.hide(page1.full_name()));
    CHECK_NOTHROW(nb.forget(page2.full_name()));
}

TEST_CASE("ttk::Treeview: tag_has")
{
    tk::Tk root;
    root.withdraw();

    ttk::Treeview tv(root);
    tv.pack();
    tv.insert("", "end", "item1", {{"text", "Item 1"}, {"tags", "special"}});
    tv.insert("", "end", "item2", {{"text", "Item 2"}});

    auto tagged = tv.tag_has("special");
    REQUIRE(tagged.size() == 1);
    CHECK(tagged[0] == "item1");
}

TEST_CASE("Toplevel/Tk: overrideredirect")
{
    tk::Tk root;
    root.withdraw();

    CHECK_FALSE(root.overrideredirect());
    root.overrideredirect(true);
    CHECK(root.overrideredirect());

    tk::Toplevel win(root);
    CHECK_FALSE(win.overrideredirect());
    win.overrideredirect(true);
    CHECK(win.overrideredirect());
}
