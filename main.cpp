#include <thread>
#include "cpp_tk.hpp"

int main()
{
    auto th = std::thread{[](){
        Interpreter interp;

        interp.evaluate("wm title . \"Minimal Tk GUI\"");
        interp.evaluate("wm geometry . 300x200");
        interp.evaluate("wm protocol . WM_DELETE_WINDOW {exit}");

        Frame frame(&interp);
        frame.pack();

        Label lbl(&frame);
        lbl.text("LLLL")
            .pack();

        Button btn(&frame);
        btn.text("Click Me")
            .command([&lbl](){ 
                std::cout << "clicked!" << std::endl; 
                lbl.text("CLICKED").config();
            })
            .pack();

        Toplevel win(&interp);
        win.title("Sub Window")
            .geometry("400x300")
            .protocol("WM_DELETE_WINDOW", "exit");

        interp.evaluate("vwait forever");
    }};

    th.join();
    return 0;
}