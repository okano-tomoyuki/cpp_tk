// Radiobutton::variable(Var&) takes a non-const reference, so passing a temporary (rvalue)
// must fail to compile (see docs/tasks.md section C-3).
#include "cpp_tk.hpp"

namespace tk = cpp_tk;

int main()
{
    tk::Tk root;
    tk::Radiobutton rb(root);
    rb.variable(tk::StringVar()); // temporary object -> should fail to compile
    return 0;
}
