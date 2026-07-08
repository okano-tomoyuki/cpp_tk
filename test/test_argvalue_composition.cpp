// ArgValueのLIST/DICT(JSON的な相互ネストを許容する合成)と、それを簡潔に組み立てる
// tk::list()/tk::dict()ヘルパー(docs/tasks.md K節参照)の回帰テスト。
// -filetypes相当の「[名前, 拡張子群]のペアを並べたリスト」がtk::list()だけで正しく組めること、
// DICTがTclのdictとして正しく機能すること、両者を相互にネストできることを、実際にTclへ渡して
// llength/lindex/dict getで検証する。
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "cpp_tk.hpp"

namespace tk = cpp_tk;

TEST_CASE("ArgValue: tk::list()で入れ子になったfiletypes相当の構造を組める")
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

TEST_CASE("ArgValue: tk::dict()がTclのdictとして正しく機能する")
{
    tk::Tk root;
    tk::ttk::Style style;

    tk::ArgValue d = tk::dict({{"alpha", 1}, {"beta", "two"}});

    CHECK(d.type() == tk::ArgValue::ValueType::DICT);
    CHECK(style.call({"dict", "get", d, "alpha"}) == "1");
    CHECK(style.call({"dict", "get", d, "beta"}) == "two");
}

TEST_CASE("ArgValue: LISTの中にDICT、DICTの中にLISTを相互にネストできる")
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
