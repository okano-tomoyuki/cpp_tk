// cpp_tk::custom::simpledialog::askstring()のEscapeキーキャンセルパスの回帰テスト。
// 単独の.cpp/実行ファイルに分離している理由はtest_simpledialog_return.cpp冒頭のコメントを参照
// (askstring()を同一プロセス内で2回連続実行するとハングする現象が判明したため)。
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "cpp_tk.hpp"
#include "custom.hpp"

namespace tk     = cpp_tk;
namespace custom = tk::custom;

TEST_CASE("simpledialog::askstring: Escapeキーでキャンセルされる")
{
    tk::Tk root;
    root.geometry("1x1-3000-3000");

    root.after(100, [&root]() {
        auto dialogs = root.winfo_children();
        if (!dialogs.empty())
        {
            auto dialog = root.nametowidget(dialogs.back());
            dialog.event_generate("<Escape>");
        }
    });

    bool cancelled = false;
    auto result = custom::simpledialog::askstring(root, "Title", "Prompt:", "initial", &cancelled);

    CHECK(cancelled);
    CHECK(result.empty());
}
