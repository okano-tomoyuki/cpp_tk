// 「未初期化オブジェクトへのcall()」の回帰テスト。他のテストファイルから分離している理由:
// PhotoImage/font::Font/ttk::Style/StringVar等は、かつては`const Widget& parent`を明示的に
// 渡す構築方式だったが、実際には内部で一切parentを参照せず、呼び出しスレッドのcurrent
// interpreter(current_interp())だけを見ていた(実態に合わせてparentは廃止した)。
// これにより「Tkがまだ無い状態でこれらを構築すると未初期化のプレースホルダになる」という
// 挙動は、プロセス内で*一度もtk::Tkを構築していない*場合にしか成立しなくなった
// (interp_map[thread_id]は一度tk::Tkを構築すると、そのtk::Tkがスコープを抜けた後も
// (明示的なdestroy()を挟まない限り)有効なまま残り続けるため、同一プロセス内の他のTEST_CASEで
// 既にtk::Tkを構築していると、current_interp()はnullptrを返さなくなる)。
// そのためこのファイルでは他のTEST_CASEを一切置かず、tk::Tkも一度も構築しない。
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "cpp_tk.hpp"

namespace tk  = cpp_tk;
namespace ttk = tk::ttk;

TEST_CASE("未初期化オブジェクトへのcall()はErrorを送出し、戻り値はfalse/空になる")
{
    tk::set_error_policy(tk::ErrorPolicy::DEFAULT);

    tk::Button uninit_btn;
    bool ok = true;
    auto ret = uninit_btn.call({"winfo", "exists", "."}, &ok);
    CHECK_FALSE(ok);
    CHECK(ret.empty());

    tk::PhotoImage uninit_img;
    ok = true;
    uninit_img.call({"foo"}, &ok);
    CHECK_FALSE(ok);

    tk::font::Font uninit_font;
    ok = true;
    uninit_font.call({"foo"}, &ok);
    CHECK_FALSE(ok);

    ttk::Style uninit_style;
    ok = true;
    uninit_style.call({"foo"}, &ok);
    CHECK_FALSE(ok);

    // Var::get_var()はbool* success相当の引数を取らないため、DEFAULT(全カテゴリでError送出)下では
    // 未初期化アクセスも例外になる。ログのみで継続させたい場合はLENIENT_CALLを指定する。
    tk::StringVar uninit_var;
    CHECK_THROWS_AS(uninit_var.get_var(), const tk::Error&);

    tk::set_error_policy(tk::ErrorPolicy::LENIENT_CALL);
    CHECK(uninit_var.get_var().empty());
    tk::set_error_policy(tk::ErrorPolicy::DEFAULT);
}
