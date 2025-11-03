#include <thread>
#include "cpp_tk.hpp"
#include "cpp_ttk.hpp"

int main()
{
    auto th = std::thread{[](){
        auto tk         = cpp_tk::Tk();

        auto frame      = cpp_tk::Frame(&tk);
        frame
            .pack();

        auto label      = cpp_ttk::Label(&frame);
        label
            .text("LLLL")
            .pack();

        auto entry      = cpp_ttk::Entry(&frame);
        entry
            .set("Hello World")
            .icursor("5")
            .pack();

        auto listbox    = cpp_tk::Listbox(&frame);
        listbox
            .insert(0, "Apple")
            .insert(1, "Banana")
            .insert(2, "Cherry")
            .pack();

        auto text       = cpp_tk::Text(&frame);
        text.pack(" -side left -fill both -expand true");

        auto scrollbar  = cpp_tk::Scrollbar(&frame);
        scrollbar.pack(" -side right -fill y");

        text.yscrollcommand([&scrollbar](const std::string& arg){
            scrollbar.set(arg);
        });

        scrollbar.command([&text](const std::string& arg){
            text.yview(arg);
        });

        auto button     = cpp_ttk::Button(&frame);
        button
            .text("Click Me")
            .command([&text](){ 
                auto file = cpp_tk::filedialog::askopenfile();
                text.insert(cpp_tk::END, file);
                std::cout << file << std::endl;
            })
            .pack();

        auto toplevel   = cpp_tk::Toplevel(&tk);
        toplevel
            .title("Sub Window")
            .geometry("400x300");

        auto canvas     = cpp_tk::Canvas(&toplevel);
        canvas
            .width(100)
            .height(100)
            .pack();
        
            canvas.create_rectangle(10, 10, 30, 30);

        int total_px    = 0;
        
        auto after_func = [&canvas, &total_px](){
            std::cout << "after" << std::endl;
            total_px += 100;
            canvas.create_oval(total_px, total_px, total_px+100, total_px+100);
        };

        canvas.after(1000, after_func);

        auto notebook   = cpp_ttk::Notebook(&toplevel);
        notebook.pack();

        auto page1      = cpp_tk::Frame(&notebook);
        page1
            .width(200)
            .height(200)
            .pack();

        auto button_p1  = cpp_tk::Button(&page1);
        button_p1
            .text("Button1")
            .pack();

        auto page2      = cpp_tk::Frame(&notebook);
        page2
            .width(200)
            .height(200)
            .pack();

        auto combo_p2   = cpp_ttk::Combobox(&page2);
        combo_p2
            .values({"AAA", "BBB", "CCC"})
            .pack();

        auto scale_p2   = cpp_tk::Scale(&page2);
        scale_p2
            .from(1.0)
            .to(10.0)
            .orient(cpp_tk::HORIZONTAL)
            .command([](const double& val){
                std::cout << val << std::endl;
            })
            .pack();

        notebook.add_tab(page1, "page1");
        notebook.add_tab(page2, "page2");

        tk.mainloop();
    }};

    th.join();
    return 0;
}