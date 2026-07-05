// cpp_tk::custom::simpledialog::askstring()のReturnキー確定パスの回帰テスト。
// 単独の.cpp/実行ファイルに分離している理由: askstring()はgrab_set()を伴うモーダルダイアログを
// 生成するため、test_simpledialog_escape.cppの同種テストと同一プロセス内で連続実行すると
// (実行順によらず)2回目のwait_window()が返らずハングする現象が判明した(docs/tasks.md
// 第5回棚卸しの追記参照)。原因はgrab関連のプラットフォーム側状態が単一プロセス内での
// 2回目のTk_Init系サイクルに影響するためと推測されるが、cpp_tk自体の不具合ではなく
// 既存のテストプロセス分離方針(test/CMakeLists.txt冒頭のコメント参照)の妥当性を裏付ける
// 事例のため、素直にプロセスを分けることで回避する。
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "cpp_tk.hpp"
#include "custom.hpp"

namespace tk     = cpp_tk;
namespace custom = tk::custom;

TEST_CASE("simpledialog::askstring: Returnキーで確定した入力値を返す")
{
    tk::Tk root;
    root.geometry("1x1-3000-3000"); // withdrawだと合成イベントが配送されないため画面外配置にする

    // ダイアログはaskstring()呼び出しの内部で生成されるため、外からは直接ハンドルを
    // 持てない。root.after()でイベントループに再入させ、ウィジェットツリーを辿って
    // Entryへ値を設定し<Return>を発火させることでOK確定を模擬する。
    // 注意: このコールバックはTcl側から起動される(invoke_guarded経由で例外は握りつぶされる)ため、
    // REQUIRE/CHECK等のdoctestマクロ(失敗時に例外を投げる)を中で使うと、失敗時にdialogが
    // 閉じられないままwait_window()が永久に返らずハングする。そのため中では例外を投げず、
    // 想定外の状態でも必ずdialogを閉じてwait_window()を返すようにし、判定は呼び出し元のCHECKに委ねる。
    root.after(100, [&root]() {
        auto dialogs = root.winfo_children();
        if (!dialogs.empty())
        {
            auto dialog = root.nametowidget(dialogs.back());
            std::string entry_name;
            for (auto& child : dialog.winfo_children())
            {
                auto w = root.nametowidget(child);
                if (w.winfo_class() == "Entry") { entry_name = child; break; }
            }
            if (!entry_name.empty())
            {
                // Entryのtextvariableを直接書き換えてからReturnを発火する(<Return>はentryにbindされている)。
                dialog.call({entry_name, "delete", "0", "end"});
                dialog.call({entry_name, "insert", "0", "hello"});
                root.nametowidget(entry_name).event_generate("<Return>");
                return;
            }
            dialog.destroy(); // Entryが見つからない想定外ケースでもハングさせない
        }
    });

    bool cancelled = true;
    auto result = custom::simpledialog::askstring(root, "Title", "Prompt:", "initial", &cancelled);

    CHECK_FALSE(cancelled);
    CHECK(result == "hello");
}
