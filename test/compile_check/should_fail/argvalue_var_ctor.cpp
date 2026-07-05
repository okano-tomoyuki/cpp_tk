// ArgValue(Var&)は非constの参照のため、Menu::add_checkbutton等のoptionsマップに一時オブジェクト
// (右辺値)のVarを直接渡すコードはコンパイルに失敗しなければならない(docs/tasks.md C-3節参照)。
// variable()のような専用setterを経由しない、options経由の裏口を塞げているかの確認。
#include "cpp_tk.hpp"

namespace tk = cpp_tk;

int main()
{
    tk::Tk root;
    tk::Menu menu(root);
    // 一時オブジェクト(tk::BooleanVar(root))をArgValueへ変換する箇所でコンパイルエラーになるべき
    menu.add_checkbutton({{"label", "item"}, {"variable", tk::BooleanVar(root)}});
    return 0;
}
