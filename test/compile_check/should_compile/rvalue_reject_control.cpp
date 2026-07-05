// should_fail/以下の各テストの対照群。同じAPIを名前付きlvalueで正しく呼び出し、
// コンパイルが通ることを確認する(should_fail側の失敗が「右辺値だから」という意図した理由で
// 起きていることの裏付け。API自体が壊れていて何を渡しても失敗する、といった誤検知を防ぐ)。
// リンクはしない(OBJECTライブラリとしてコンパイルのみ行う)ため、実行はされない。
#include "cpp_tk.hpp"

namespace tk  = cpp_tk;
namespace ttk = tk::ttk;

void checkbutton_variable_ok(tk::Widget& root)
{
    tk::Checkbutton cb(root);
    tk::BooleanVar bv(root);
    cb.variable(bv);
}

void radiobutton_variable_ok(tk::Widget& root)
{
    tk::Radiobutton rb(root);
    tk::StringVar sv(root);
    rb.variable(sv);
}

void entry_textvariable_classic_ok(tk::Widget& root)
{
    tk::Entry entry(root);
    tk::StringVar sv(root);
    entry.textvariable(sv);
}

void entry_textvariable_ttk_ok(tk::Widget& root)
{
    ttk::Entry entry(root);
    tk::StringVar sv(root);
    entry.textvariable(sv);
}

void combobox_textvariable_ok(tk::Widget& root)
{
    ttk::Combobox combo(root);
    tk::StringVar sv(root);
    combo.textvariable(sv);
}

void spinbox_textvariable_classic_ok(tk::Widget& root)
{
    tk::Spinbox spin(root);
    tk::StringVar sv(root);
    spin.textvariable(sv);
}

void spinbox_textvariable_ttk_ok(tk::Widget& root)
{
    ttk::Spinbox spin(root);
    tk::StringVar sv(root);
    spin.textvariable(sv);
}

void argvalue_var_ctor_ok(tk::Widget& root)
{
    tk::Menu menu(root);
    tk::BooleanVar bv(root);
    menu.add_checkbutton({{"label", "item"}, {"variable", bv}});
}
