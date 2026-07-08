// Regression tests for ArgValue's LIST/DICT (JSON-like mutual nesting composition) and the
// tk::list()/tk::dict() helpers that build them concisely (see docs/tasks.md section K).
// Verifies, by actually passing values into Tcl and checking with llength/lindex/dict get, that:
// a -filetypes-style "list of [name, extensions] pairs" can be built purely with tk::list();
// DICT works correctly as a Tcl dict; and the two can be nested inside each other.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "cpp_tk.hpp"

namespace tk = cpp_tk;

TEST_CASE("ArgValue: tk::list() can build a nested structure equivalent to filetypes")
{
    tk::Tk root;
    tk::ttk::Style style;

    tk::ArgValue filetypes = tk::list({
        tk::list({"Text files", tk::list({"*.txt", "*.TXT"})}),
        tk::list({"All files", tk::list({"*"})})
    });

    CHECK(filetypes.type() == tk::ArgValue::ValueType::LIST);
    CHECK(style.call({"llength", filetypes}) == "2");

    auto first_pair = style.call({"lindex", filetypes, 0});
    CHECK(style.call({"lindex", first_pair, 0}) == "Text files");

    auto first_exts = style.call({"lindex", first_pair, 1});
    CHECK(style.call({"llength", first_exts}) == "2");
    CHECK(style.call({"lindex", first_exts, 0}) == "*.txt");

    auto second_pair = style.call({"lindex", filetypes, 1});
    CHECK(style.call({"lindex", second_pair, 0}) == "All files");
}

TEST_CASE("ArgValue: tk::dict() works correctly as a Tcl dict")
{
    tk::Tk root;
    tk::ttk::Style style;

    tk::ArgValue d = tk::dict({{"alpha", 1}, {"beta", "two"}});

    CHECK(d.type() == tk::ArgValue::ValueType::DICT);
    CHECK(style.call({"dict", "get", d, "alpha"}) == "1");
    CHECK(style.call({"dict", "get", d, "beta"}) == "two");
}

TEST_CASE("ArgValue: a DICT can be nested inside a LIST, and a LIST inside a DICT")
{
    tk::Tk root;
    tk::ttk::Style style;

    tk::ArgValue list_of_dict = tk::list({
        tk::dict({{"nums", tk::list({1, 2, 3})}})
    });

    auto first = style.call({"lindex", list_of_dict, 0});
    auto nums  = style.call({"dict", "get", first, "nums"});
    CHECK(style.call({"llength", nums}) == "3");
    CHECK(style.call({"lindex", nums, 1}) == "2");
}
