// Regression tests for custom::use_sv_ttk_theme() (embedding, extraction, and applying the Sun
// Valley ttk theme vendored under thirdparty/sv_ttk/; see docs/tasks.md section G).
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "cpp_tk.hpp"
#include "custom.hpp"

namespace tk = cpp_tk;

TEST_CASE("use_sv_ttk_theme: both dark/light switch themes without throwing, and the colors actually change")
{
    tk::Tk root;
    tk::ttk::Style style;

    // "ttk::style configure <style>" does not support querying a single option (-background
    // alone returns an empty string), so inspect the full option list instead (a string like
    // "-background #1c1c1c ...").
    CHECK_NOTHROW(tk::custom::use_sv_ttk_theme(true));
    CHECK(style.theme_use() == "sun-valley-dark");
    auto dark_config = style.call({"ttk::style", "configure", "."});
    CHECK(dark_config.find("-background") != std::string::npos);

    CHECK_NOTHROW(tk::custom::use_sv_ttk_theme(false));
    CHECK(style.theme_use() == "sun-valley-light");
    auto light_config = style.call({"ttk::style", "configure", "."});
    CHECK(light_config.find("-background") != std::string::npos);

    // The configuration (including background color) genuinely differs between dark/light -
    // confirming that more than just the theme name switched, and the spritesheet/color
    // definitions were loaded correctly too.
    CHECK(dark_config != light_config);
}

TEST_CASE("use_sv_ttk_theme: works correctly on subsequent calls without re-extracting")
{
    tk::Tk root;

    tk::custom::use_sv_ttk_theme(true);
    CHECK_NOTHROW(tk::custom::use_sv_ttk_theme(true));
    tk::ttk::Style style;
    CHECK(style.theme_use() == "sun-valley-dark");
}
