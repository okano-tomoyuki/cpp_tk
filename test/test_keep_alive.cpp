// keep_alive()(複数Widgetをまとめてhandle()経由でキャプチャするヘルパー、docs/tasks.md C節8.参照)
// の回帰テスト。コールバックが発火する時点で元のC++オブジェクトが既にスコープを抜けていても、
// keep_alive()が返した呼び出し可能オブジェクト経由でTcl側の実体へ安全にアクセスできることを確認する。
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "cpp_tk.hpp"

namespace tk = cpp_tk;

TEST_CASE("keep_alive: 単一Widgetが元オブジェクト破棄後もhandle経由で操作できる")
{
    tk::Tk root;
    std::function<void()> touch;

    {
        tk::Label label(root, {{"text", "before"}});
        auto handles = tk::keep_alive(label);
        touch = [handles]() {
            std::get<0>(handles()).config("text", "after");
        };
    } // labelはここでC++オブジェクトとして破棄されるが、Tcl側の実体はdestroy()していないため残る

    touch();

    // 破棄されたlabelのフルネームを直接知らないので、root配下から辿って確認する。
    auto children = root.winfo_children();
    REQUIRE(children.size() == 1);
    tk::Widget same = root.nametowidget(children[0]);
    CHECK(same.cget("text") == "after");
}

TEST_CASE("keep_alive: 複数Widgetをまとめてキャプチャし、std::get<N>で個別に参照できる")
{
    tk::Tk root;
    std::function<void(const std::string&)> sync;

    {
        tk::Scrollbar scrollbar(root, {{"orient", "vertical"}});
        tk::Text text(root);
        auto handles = tk::keep_alive(scrollbar, text);
        sync = [handles](const std::string& args) {
            auto widgets = handles();
            std::get<1>(widgets).insert("end", args); // text
            std::get<0>(widgets).set("0.0 1.0");       // scrollbar
        };
    } // scrollbar/textともにC++オブジェクトとしては破棄済み

    CHECK_NOTHROW(sync("hello"));
}
