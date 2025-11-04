#include <thread>
#include <iostream>
#include "cpp_tk.hpp"

int main()
{
    namespace tk            = cpp_tk;
    namespace ttk           = tk::ttk;
    namespace filedialog    = tk::filedialog;
    namespace messagebox    = tk::messagebox;

    auto th = std::thread{[](){
        auto tk         = tk::Tk();

        auto frame      = tk::Frame(&tk);
        frame
            .pack();

        auto label      = ttk::Label(&frame);
        label
            .text("LLLL")
            .pack();

        auto entry      = ttk::Entry(&frame);
        entry
            .set("Hello World")
            .icursor("5")
            .pack();

        auto listbox    = tk::Listbox(&frame);
        listbox
            .insert(0, "Apple")
            .insert(1, "Banana")
            .insert(2, "Cherry")
            .pack();

        auto text       = tk::Text(&frame);
        text.pack({{"side", "left"}, {"fill", "both"}, {"expand", "true"}});

        auto scrollbar  = tk::Scrollbar(&frame);
        scrollbar
            .pack({{"side", "right"}, {"fill", "y"}});

        text.yscrollcommand([&scrollbar](const std::string& arg){
            scrollbar
                .set(arg);
        });

        scrollbar.command([&text](const std::string& arg){
            text
                .yview(arg);
        });

        auto button     = ttk::Button(&frame);
        button
            .text("Click Me")
            .command([&text](){ 
                auto file = filedialog::askopenfile();
                text.insert(tk::END, file);
                std::cout << file << std::endl;
            })
            .pack();

        auto toplevel   = tk::Toplevel(&tk);
        toplevel
            .title("Sub Window")
            .geometry("400x300");

        auto canvas     = tk::Canvas(&toplevel);
        canvas
            .width(100)
            .height(100)
            .pack();
        
        canvas
            .create_rectangle(10, 10, 30, 30);

        int total_px    = 0;
        
        auto after_func = [&canvas, &total_px](){
            total_px += 100;
            canvas.create_oval(total_px, total_px, total_px+100, total_px+100);
        };

        canvas.after(1000, after_func);

        auto notebook   = ttk::Notebook(&toplevel);
        notebook.pack();

        auto page1      = tk::Frame(&notebook);
        page1
            .width(200)
            .height(200)
            .pack();

        auto button_p1  = ttk::Button(&page1);
        button_p1
            .text("Button1")
            .command([](){
                messagebox::showinfo("Title", "Message");
                messagebox::showwarning("Title", "Message");
                messagebox::showerror("Title", "Message");
            })
            .pack();

        auto page2      = tk::Frame(&notebook);
        page2
            .width(200)
            .height(200)
            .pack();

        auto combo_p2   = ttk::Combobox(&page2);
        combo_p2
            .values({"AAA", "BBB", "CCC"})
            .pack();

        auto scale_p2   = tk::Scale(&page2);
        scale_p2
            .from(1.0)
            .to(10.0)
            .orient(tk::HORIZONTAL)
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