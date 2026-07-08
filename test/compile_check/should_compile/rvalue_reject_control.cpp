// Control group for each test under should_fail/. Calls the same API correctly with a named
// lvalue and confirms it compiles (this confirms that the should_fail failures happen for the
// intended reason - "because it's an rvalue" - rather than a false positive where the API is
// simply broken and fails for any argument). Not linked (compiled only, as an OBJECT library),
// so this is never executed.
#include "cpp_tk.hpp"

namespace tk  = cpp_tk;
namespace ttk = tk::ttk;

void checkbutton_variable_ok(tk::Widget& root)
{
    tk::Checkbutton cb(root);
    tk::BooleanVar bv;
    cb.variable(bv);
}

void radiobutton_variable_ok(tk::Widget& root)
{
    tk::Radiobutton rb(root);
    tk::StringVar sv;
    rb.variable(sv);
}

void entry_textvariable_classic_ok(tk::Widget& root)
{
    tk::Entry entry(root);
    tk::StringVar sv;
    entry.textvariable(sv);
}

void entry_textvariable_ttk_ok(tk::Widget& root)
{
    ttk::Entry entry(root);
    tk::StringVar sv;
    entry.textvariable(sv);
}

void combobox_textvariable_ok(tk::Widget& root)
{
    ttk::Combobox combo(root);
    tk::StringVar sv;
    combo.textvariable(sv);
}

void spinbox_textvariable_classic_ok(tk::Widget& root)
{
    tk::Spinbox spin(root);
    tk::StringVar sv;
    spin.textvariable(sv);
}

void spinbox_textvariable_ttk_ok(tk::Widget& root)
{
    ttk::Spinbox spin(root);
    tk::StringVar sv;
    spin.textvariable(sv);
}

void argvalue_var_ctor_ok(tk::Widget& root)
{
    tk::Menu menu(root);
    tk::BooleanVar bv;
    menu.add_checkbutton({{"label", "item"}, {"variable", bv}});
}
