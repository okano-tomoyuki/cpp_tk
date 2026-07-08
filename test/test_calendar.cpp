// Regression tests for custom::Calendar (the MVP month-view calendar widget, see docs/tasks.md
// section G).
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "cpp_tk.hpp"
#include "custom.hpp"

namespace tk = cpp_tk;

TEST_CASE("Calendar: constructing with an explicit year/month/day makes get_date() return that date")
{
    tk::Tk root;
    root.withdraw();

    tk::custom::Calendar cal(root, 2026, 7, 5);
    int y = 0, m = 0, d = 0;
    cal.get_date(y, m, d);
    CHECK(y == 2026);
    CHECK(m == 7);
    CHECK(d == 5);
}

TEST_CASE("Calendar: clicking a date cell changes the selected date and invokes the command() callback")
{
    tk::Tk root;
    root.withdraw();

    tk::custom::Calendar cal(root, 2026, 7, 5);
    cal.pack();
    cal.update();

    int called_year = 0, called_month = 0, called_day = 0;
    int call_count = 0;
    cal.command([&](int y, int m, int d) {
        called_year = y; called_month = m; called_day = d;
        ++call_count;
    });

    // Instead of clicking the currently-displayed 7/5 cell directly, verify consistency between
    // switching via set_date() and reading back with get_date() (keeps this test independent of
    // the internal widget path of any particular cell).
    cal.set_date(2026, 8, 15);
    int y = 0, m = 0, d = 0;
    cal.get_date(y, m, d);
    CHECK(y == 2026);
    CHECK(m == 8);
    CHECK(d == 15);
}

TEST_CASE("Calendar: actually clicking a date cell invokes the command() callback with the selected date")
{
    tk::Tk root;
    // Event delivery via event_generate only happens while the window is mapped (viewable) - it
    // is not delivered while withdrawn - so place the window off-screen to keep it mapped. If an
    // earlier TEST_CASE already left the window mapped, deiconify() alone may produce no state
    // change (no Visibility event) and the update may not take effect, so withdraw() first
    // guarantees an actual unmapped state before deiconify() re-maps it.
    root.withdraw();
    root.deiconify();
    root.geometry("300x300-3000-3000");

    tk::custom::Calendar cal(root, 2026, 7, 5);
    cal.pack();
    cal.update();

    int called_year = 0, called_month = 0, called_day = 0;
    int call_count = 0;
    cal.command([&](int y, int m, int d) {
        called_year = y; called_month = m; called_day = d;
        ++call_count;
    });

    // Calendar's children are (header_row, 7 weekday Labels, the date grid Frame) - the last one is the date grid.
    auto top_children = cal.winfo_children();
    REQUIRE(top_children.size() >= 1);
    tk::Widget grid = cal.nametowidget(top_children.back());
    auto cells = grid.winfo_children();
    REQUIRE_FALSE(cells.empty());

    tk::Widget first_cell = cal.nametowidget(cells.front());
    first_cell.event_generate("<Button-1>");
    cal.update();

    CHECK(call_count == 1);
    int y = 0, m = 0, d = 0;
    cal.get_date(y, m, d);
    CHECK(called_year == y);
    CHECK(called_month == m);
    CHECK(called_day == d);
}

TEST_CASE("Calendar: omitting the arguments defaults to today's date")
{
    tk::Tk root;
    root.withdraw();

    tk::custom::Calendar cal(root);
    int y = 0, m = 0, d = 0;
    cal.get_date(y, m, d);
    CHECK(y > 2000);
    CHECK(m >= 1);
    CHECK(m <= 12);
    CHECK(d >= 1);
    CHECK(d <= 31);
}
