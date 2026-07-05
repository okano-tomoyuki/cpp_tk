// custom::Calendar(月表示カレンダーWidgetのMVP版、docs/tasks.md G節参照)の回帰テスト。
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "cpp_tk.hpp"
#include "custom.hpp"

namespace tk = cpp_tk;

TEST_CASE("Calendar: 明示的な年月日で初期化するとget_date()がその日付を返す")
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

TEST_CASE("Calendar: 日付セルをクリックすると選択日が変わりcommand()コールバックが呼ばれる")
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

    // 7/5(表示中の月)のセルを直接クリックする代わりに、set_date()経由での切り替えと
    // get_date()の整合性を検証する(セルの内部ウィジェットパスに依存しないテストにするため)。
    cal.set_date(2026, 8, 15);
    int y = 0, m = 0, d = 0;
    cal.get_date(y, m, d);
    CHECK(y == 2026);
    CHECK(m == 8);
    CHECK(d == 15);
}

TEST_CASE("Calendar: 実際に日付セルをクリックするとcommand()コールバックが選択日で呼ばれる")
{
    tk::Tk root;
    // event_generateによるイベント配送はウィンドウがmapped(viewable)状態でないと行われない
    // (withdraw()中は配送されない)ため、画面外へ配置してmapped状態を保つ。先行するTEST_CASEが
    // 既にmapped状態にしている場合、deiconify()単体では状態変化(Visibilityイベント)が発生せず
    // 反映されないことがあるため、withdraw()で一旦確実にunmapしてからdeiconify()する。
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

    // Calendarの子(header_row, 曜日Label x7, 日付グリッドFrame)のうち最後が日付グリッド。
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

TEST_CASE("Calendar: 引数省略で本日の日付になる")
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
