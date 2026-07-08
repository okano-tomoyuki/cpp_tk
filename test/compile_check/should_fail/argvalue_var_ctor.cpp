// ArgValue(Var&) takes a non-const reference, so passing a temporary (rvalue) Var directly into
// an options map like Menu::add_checkbutton's must fail to compile (see docs/tasks.md
// section C-3). Confirms this backdoor via options (bypassing a dedicated setter like
// variable()) is properly closed off.
#include "cpp_tk.hpp"

namespace tk = cpp_tk;

int main()
{
    tk::Tk root;
    tk::Menu menu(root);
    // Should fail to compile where the temporary tk::BooleanVar() is converted to ArgValue
    menu.add_checkbutton({{"label", "item"}, {"variable", tk::BooleanVar()}});
    return 0;
}
