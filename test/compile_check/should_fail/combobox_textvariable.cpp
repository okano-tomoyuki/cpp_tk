// ttk::Combobox::textvariable(StringVar&) takes a non-const reference, so passing a temporary
// (rvalue) must fail to compile (see docs/tasks.md section C-3).
#include "cpp_tk.hpp"

namespace tk  = cpp_tk;
namespace ttk = tk::ttk;

int main()
{
    tk::Tk root;
    ttk::Combobox combo(root);
    combo.textvariable(tk::StringVar()); // temporary object -> should fail to compile
    return 0;
}
