// ttk::Entry::textvariable(StringVar&)は非constの参照のため、一時オブジェクト(右辺値)を
// 渡すコードはコンパイルに失敗しなければならない(docs/tasks.md C-3節参照)。
#include "cpp_tk.hpp"

namespace tk  = cpp_tk;
namespace ttk = tk::ttk;

int main()
{
    tk::Tk root;
    ttk::Entry entry(root);
    entry.textvariable(tk::StringVar(root)); // 一時オブジェクト → コンパイルエラーになるべき
    return 0;
}
