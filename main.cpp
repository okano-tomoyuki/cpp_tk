#include <thread>
#include "cpp_tk.hpp"

int main()
{
    auto th = std::thread{[](){
        auto tk     = Tk();

        auto frame  = Frame(&tk);
        frame
            .pack();

        auto label  = Label(&frame);
        label
            .text("LLLL")
            .pack();

        auto entry  = Entry(&frame);
        entry
            .pack();

        auto button = Button(&frame);
        button
            .text("Click Me")
            .command([&label, &entry](){ 
                std::cout << entry.get() << std::endl; 
                label.text("CLICKED");
            })
            .pack();

        auto toplevel   = Toplevel(&tk);
        toplevel
            .title("Sub Window")
            .geometry("400x300");

        tk.mainloop();
    }};

    th.join();
    return 0;
}