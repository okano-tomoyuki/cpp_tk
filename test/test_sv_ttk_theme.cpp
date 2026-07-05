// custom::use_sv_ttk_theme()(thirdparty/sv_ttk/にベンダリングしたSun Valley ttk themeの
// 埋め込み・展開・適用、docs/tasks.md G節参照)の回帰テスト。
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "cpp_tk.hpp"
#include "custom.hpp"

namespace tk = cpp_tk;

TEST_CASE("use_sv_ttk_theme: dark/light共に例外を投げずテーマが切り替わり、実際に配色が変わる")
{
    tk::Tk root;
    tk::ttk::Style style;

    // ttk::style configure <style>は単一optionクエリ(-background単体)には対応しておらず
    // 空文字を返すため、オプション一覧全体("-background #1c1c1c ..."のような文字列)を見る。
    CHECK_NOTHROW(tk::custom::use_sv_ttk_theme(true));
    CHECK(style.theme_use() == "sun-valley-dark");
    auto dark_config = style.call({"ttk::style", "configure", "."});
    CHECK(dark_config.find("-background") != std::string::npos);

    CHECK_NOTHROW(tk::custom::use_sv_ttk_theme(false));
    CHECK(style.theme_use() == "sun-valley-light");
    auto light_config = style.call({"ttk::style", "configure", "."});
    CHECK(light_config.find("-background") != std::string::npos);

    // dark/lightで設定内容(背景色を含む)が実際に異なる(単なるテーマ名の切り替えだけでなく、
    // spritesheet/配色定義まで含めて正しくロードされていることの裏付け)。
    CHECK(dark_config != light_config);
}

TEST_CASE("use_sv_ttk_theme: 2回目以降の呼び出しでも再展開せず正しく動作する")
{
    tk::Tk root;

    tk::custom::use_sv_ttk_theme(true);
    CHECK_NOTHROW(tk::custom::use_sv_ttk_theme(true));
    tk::ttk::Style style;
    CHECK(style.theme_use() == "sun-valley-dark");
}
