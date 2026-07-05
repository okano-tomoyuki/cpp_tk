// current_interp()の遅延生成の回帰テスト。他のテストファイルから分離している理由:
// このプロセスでは*一度もtk::Tkを構築していない*状態を保つ必要があるため
// (一度でもtk::Tkを構築すると、そのInterpreterがプロセス内に残り続け、以降は
// 「まだ何も構築していない」状態を再現できなくなる)。
//
// Var/PhotoImage/font::Font/ttk::Styleはparentを取らず、構築時にcurrent_interp()へ
// 束縛するが、current_interp()は当該スレッドにInterpreterが無ければその場で生成するため、
// tk::Tkを一度も構築していなくてもこれらのオブジェクトは常に実体化される
// (docs/tasks.md参照)。一方、Widget派生クラスは実の親を渡さない限りプレースホルダの
// ままであり、この挙動には影響されない。
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "cpp_tk.hpp"

namespace tk  = cpp_tk;
namespace ttk = tk::ttk;

TEST_CASE("親を渡さないWidgetはtk::Tkが一度も無くてもプレースホルダのままcall()が失敗する")
{
    tk::set_error_policy(tk::ErrorPolicy::DEFAULT);

    tk::Button uninit_btn;
    bool ok = true;
    auto ret = uninit_btn.call({"winfo", "exists", "."}, &ok);
    CHECK_FALSE(ok);
    CHECK(ret.empty());
}

TEST_CASE("Var/PhotoImage/font::Font/ttk::Styleはtk::Tkが一度も無くてもcurrent_interp()経由で自動的に実体化される")
{
    tk::set_error_policy(tk::ErrorPolicy::DEFAULT);

    tk::PhotoImage img;
    bool ok = false;
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

    tk::StringVar var;
    CHECK(var.get() == "");
}
