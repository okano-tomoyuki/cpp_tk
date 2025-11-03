#include "cpp_tk.hpp"

int main() 
{
    namespace tk         = cpp_tk;
    namespace ttk        = tk::ttk;
    namespace messagebox = tk::messagebox;

    tk::Tk root;
    tk::Button btn(&root);
    btn.text("Click Me").command([]() {
        messagebox::showinfo("Hello", "Button clicked!");
    });
    btn.pack();
    root.mainloop();
}