// Regression tests for application/misc-level native Tcl commands (4th inventory pass):
// windowingsystem/bell/wait_visibility/scaling/bindtags/image_names/image_types/
// PhotoImage & BitmapImage::destroy()
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "cpp_tk.hpp"

#include <thread>
#include <chrono>

namespace tk = cpp_tk;

TEST_CASE("windowingsystem: returns one of x11/win32/aqua")
{
    tk::Tk root;
    root.withdraw();

    auto ws = root.windowingsystem();
    CHECK((ws == "x11" || ws == "win32" || ws == "aqua"));
}

TEST_CASE("bell: does not throw")
{
    tk::Tk root;
    root.withdraw();
    CHECK_NOTHROW(root.bell());
}

TEST_CASE("wait_visibility: the window is mapped by the time it returns")
{
    tk::Tk root;
    // An earlier TEST_CASE may have already left the window mapped, in which case deiconify()
    // alone produces no state change (no Visibility event) and wait_visibility() would hang.
    // withdraw() first guarantees an actual unmapped state before deiconify() re-maps it, so a
    // change always occurs.
    root.withdraw();
    root.deiconify();
    root.geometry("50x50-3000-3000"); // withdraw alone would not become mapped, so place off-screen instead

    root.wait_visibility();
    CHECK(root.winfo_ismapped());
}

TEST_CASE("scaling: getter/setter work correctly")
{
    tk::Tk root;
    root.withdraw();

    auto original = root.scaling();
    CHECK(original > 0);

    root.scaling(1.5);
    // Tk rounds internally to whole pixels, so exact floating-point equality cannot be expected;
    // allow a generous tolerance (this is Tk's own rounding behavior, not a cpp_tk defect).
    CHECK(root.scaling() == doctest::Approx(1.5).epsilon(0.01));

    root.scaling(original); // restore, so as not to affect other tests
}

TEST_CASE("bindtags: getter/setter work correctly")
{
    tk::Tk root;
    root.withdraw();
    tk::Button btn(root);
    btn.pack();

    auto tags = btn.bindtags();
    CHECK(tags.size() == 4); // default is [self, class, toplevel, all]

    std::vector<std::string> custom_tags = {btn.full_name(), "all"};
    btn.bindtags(custom_tags);
    CHECK(btn.bindtags() == custom_tags);
}

TEST_CASE("image_names/image_types: a created image appears in the listing and disappears after destroy()")
{
    tk::Tk root;
    root.withdraw();

    auto types = tk::image_types();
    bool has_photo = false, has_bitmap = false;
    for (auto& t : types) { if (t == "photo") has_photo = true; if (t == "bitmap") has_bitmap = true; }
    CHECK(has_photo);
    CHECK(has_bitmap);

    tk::PhotoImage img;
    auto names_before = tk::image_names();
    bool found = false;
    for (auto& n : names_before) if (n == img.name()) found = true;
    CHECK(found);

    img.destroy();
    auto names_after = tk::image_names();
    found = false;
    for (auto& n : names_after) if (n == img.name()) found = true;
    CHECK_FALSE(found);
}
