// Widget共通機能(ジオメトリマネージャ/config・cget/bind・unbind/after系/winfo系/
// focus系/clipboard系/wait系/lift・lower)の回帰テスト。これまで自動テストが存在せず、
// 最も基礎的かつ利用頻度の高い部分であるため優先して整備する。
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "cpp_tk.hpp"

namespace tk = cpp_tk;

TEST_CASE("pack/pack_forget: winfo_managerが追従する")
{
    tk::Tk root;
    root.withdraw();
    tk::Frame f(root);

    CHECK(f.winfo_manager().empty());
    f.pack();
    CHECK(f.winfo_manager() == "pack");
    f.pack_forget();
    CHECK(f.winfo_manager().empty());
}

TEST_CASE("grid/grid_forget: winfo_managerが追従する")
{
    tk::Tk root;
    root.withdraw();
    tk::Frame f(root);

    f.grid();
    CHECK(f.winfo_manager() == "grid");
    f.grid_forget();
    CHECK(f.winfo_manager().empty());
}

TEST_CASE("place/place_forget: winfo_managerが追従する")
{
    tk::Tk root;
    root.withdraw();
    tk::Frame f(root);

    f.place({{"x", 0}, {"y", 0}});
    CHECK(f.winfo_manager() == "place");
    f.place_forget();
    CHECK(f.winfo_manager().empty());
}

TEST_CASE("config/cget: 設定した値が読み出せる")
{
    tk::Tk root;
    root.withdraw();
    tk::Label label(root);

    label.config("text", "hello");
    CHECK(label.cget("text") == "hello");

    label.config({{"text", "world"}, {"width", 10}});
    CHECK(label.cget("text") == "world");
    CHECK(label.cget("width") == "10");
}

TEST_CASE("grid_rowconfigure/grid_columnconfigure: 例外にならない")
{
    tk::Tk root;
    root.withdraw();
    tk::Frame f(root);
    f.pack();

    CHECK_NOTHROW(f.grid_rowconfigure(0, {{"weight", 1}}));
    CHECK_NOTHROW(f.grid_columnconfigure(0, {{"weight", 1}}));

    // rootは同一プロセス内の他のTEST_CASEでも再利用されるため、pack()したままにすると
    // 後続のTEST_CASEでrootのpack領域を奪い合ってしまう。使い終わったら明示的に破棄する。
    f.destroy();
}

TEST_CASE("bind/unbind: bindしたコールバックが発火し、unbind後は発火しない")
{
    tk::Tk root;
    // event generate によるbindへのイベント配送は、ウィンドウがmapped(viewable)状態でないと
    // 行われない(withdraw()中は配送されないことを実機で確認した)。withdraw()の代わりに
    // 画面外へ配置してmapped状態を保つ。先行するTEST_CASEが既にmapped状態にしている場合、
    // deiconify()単体では状態変化(Visibilityイベント)が発生せずwait_visibility()がハングする
    // ため、withdraw()で一旦確実にunmapしてからdeiconify()することで必ず変化を起こす。
    root.withdraw();
    root.deiconify();
    root.geometry("1x1-3000-3000");
    root.wait_visibility(); // deiconify()の反映(実際にmappedになるまで)を待つ
    tk::Frame f(root);
    f.pack();
    f.update(); // bindを有効にするためウィジェットの生成を確定させる

    int fired = 0;
    f.bind("<<TestEvent>>", [&](const tk::Event&) { ++fired; });

    f.event_generate("<<TestEvent>>");
    f.update();
    CHECK(fired == 1);

    f.unbind("<<TestEvent>>");
    f.event_generate("<<TestEvent>>");
    f.update();
    CHECK(fired == 1); // unbind後は増えない
}

TEST_CASE("after: 指定ミリ秒後にコールバックが実行される(mainloop経由)")
{
    tk::Tk root;
    root.withdraw();

    bool fired = false;
    root.after(10, [&]() {
        fired = true;
        root.quit();
    });
    root.mainloop();

    CHECK(fired);
}

TEST_CASE("after_cancel: キャンセルしたコールバックは実行されない")
{
    tk::Tk root;
    root.withdraw();

    bool cancelled_fired = false;
    auto id = root.after(20, [&]() { cancelled_fired = true; });
    root.after_cancel(id);

    // cancelled_fired用のタイマーより後にquit()するタイマーを別途仕込み、
    // その時点でcancelled_firedがfalseのままであることを確認する。
    root.after(60, [&]() { root.quit(); });
    root.mainloop();

    CHECK_FALSE(cancelled_fired);
}

TEST_CASE("after_idle: アイドル時にコールバックが実行される")
{
    tk::Tk root;
    root.withdraw();

    bool fired = false;
    root.after_idle([&]() {
        fired = true;
        root.quit();
    });
    root.mainloop();

    CHECK(fired);
}

TEST_CASE("destroy/winfo_exists: destroy後はwinfo_existsがfalseになる")
{
    tk::Tk root;
    root.withdraw();
    tk::Frame f(root);
    f.pack();

    CHECK(f.winfo_exists());
    f.destroy();
    root.update();
    CHECK_FALSE(f.winfo_exists());
}

TEST_CASE("winfo_class/winfo_toplevel/winfo_children: 基本情報が取得できる")
{
    tk::Tk root;
    root.withdraw();
    tk::Frame f(root);
    f.pack();
    tk::Label child(f);
    child.pack();

    CHECK(f.winfo_class() == "Frame");
    CHECK(f.winfo_toplevel() == root.full_name());

    auto children = f.winfo_children();
    CHECK(children.size() == 1);
    CHECK(children[0] == child.full_name());
}

TEST_CASE("winfo_width/height/x/y/rootx/rooty: 例外にならず整数を返す")
{
    tk::Tk root;
    root.withdraw();
    tk::Frame f(root);
    f.pack();
    f.update();

    CHECK_NOTHROW(f.winfo_width());
    CHECK_NOTHROW(f.winfo_height());
    CHECK_NOTHROW(f.winfo_x());
    CHECK_NOTHROW(f.winfo_y());
    CHECK_NOTHROW(f.winfo_rootx());
    CHECK_NOTHROW(f.winfo_rooty());
}

TEST_CASE("winfo_screenwidth/screenheight: 正の値を返す")
{
    tk::Tk root;
    root.withdraw();

    CHECK(root.winfo_screenwidth() > 0);
    CHECK(root.winfo_screenheight() > 0);
}

TEST_CASE("winfo_pointerx/pointery: 例外にならない")
{
    tk::Tk root;
    root.withdraw();

    CHECK_NOTHROW(root.winfo_pointerx());
    CHECK_NOTHROW(root.winfo_pointery());
}

TEST_CASE("winfo_ismapped: withdraw中はfalseのまま")
{
    tk::Tk root;
    root.withdraw();
    tk::Frame f(root);
    f.pack();
    f.update();

    // 親のrootをwithdrawしている間は子もマッピングされない。
    CHECK_FALSE(f.winfo_ismapped());
}

TEST_CASE("clipboard_clear/append/get: 設定した文字列が読み出せる")
{
    tk::Tk root;
    root.withdraw();

    root.clipboard_clear();
    root.clipboard_append("hello clipboard");
    CHECK(root.clipboard_get() == "hello clipboard");
}

TEST_CASE("wait_variable: Varが書き込まれるまでブロックし、書き込まれたら戻る")
{
    tk::Tk root;
    root.withdraw();
    tk::StringVar var;

    // wait_variable()自体はネストしたイベントループでブロックするため、ブロック中も
    // Tclのタイマーイベントは処理される。事前に仕掛けたafter()がその間に発火してvarを書き換える。
    root.after(10, [&]() { var.set("changed"); });
    root.wait_variable(var);

    CHECK(var.get() == "changed");
}

TEST_CASE("wait_window: ウィジェットが破棄されるまでブロックし、破棄されたら戻る")
{
    tk::Tk root;
    root.withdraw();
    tk::Toplevel win(root);

    bool destroyed_before_return = false;
    root.after(10, [&]() {
        win.destroy();
        destroyed_before_return = true;
    });
    win.wait_window();

    CHECK(destroyed_before_return);
}

TEST_CASE("lift/lower: 例外にならない")
{
    tk::Tk root;
    root.withdraw();
    tk::Frame f1(root);
    tk::Frame f2(root);
    f1.place({{"x", 0}, {"y", 0}});
    f2.place({{"x", 0}, {"y", 0}});

    CHECK_NOTHROW(f1.lift());
    CHECK_NOTHROW(f2.lower());
}

TEST_CASE("as_parent: 親と同じ具象型を渡す場合のコンパイルエラーをas_parent()経由で回避できる")
{
    tk::Tk root;
    root.withdraw();
    tk::Frame parent(root);

    // tk::Frame child(parent); は Frame(const Frame&)(コピー禁止によりdelete済み)が
    // 完全一致としてオーバーロード解決で選ばれてしまいコンパイルエラーになる
    // (docs/tasks.md I節参照)。as_parent()で静的型をWidget&に変えることで回避する。
    tk::Frame child(tk::as_parent(parent));
    child.pack();

    CHECK(child.winfo_parent() == parent.full_name());
}
