#include <fstream>
#include <iostream>
#include <cpp_tk.hpp>

int main()
{
    namespace tk  = cpp_tk;
    namespace ttk = tk::ttk;

    // --- Tk 初期化 ---
    tk::Tk root;

    // --- Treeview 作成 ---
    ttk::Treeview tv(&root, {{"columns", "name age"}});

    // 列ヘッダ
    tv.heading("#0", {{"text", "ID"}});
    tv.heading("name", {{"text", "Name"}});
    tv.heading("age",  {{"text", "Age"}});

    // 列幅
    tv.column("#0",   {{"width", 80}});
    tv.column("name", {{"width", 120}});
    tv.column("age",  {{"width", 80}});

    // --- ノード挿入 ---
    tv.insert("", "end", "item1", {{"text", "001"}, {"values", "Alice 20"}});
    tv.insert("", "end", "item2", {{"text", "002"}, {"values", "Bob 30"}});
    tv.insert("item1", "end", "child1", {{"text", "001-1"}, {"values", "Child 5"}});

    // --- set() のテスト ---
    tv.set("item2", "age", 35);

    // --- move() のテスト（item2 を item1 の子へ移動） ---
    tv.move("item2", "item1", "end");

    // --- detach / reattach のテスト ---
    tv.detach("child1");
    tv.reattach("child1", "", "end");

    // --- get_children のテスト ---
    auto children = tv.get_children("");
    std::cout << "Root children: ";
    for (auto& c : children) std::cout << c << " ";
    std::cout << std::endl;

    // --- parent / index のテスト ---
    std::cout << "parent(item2) = " << tv.parent("item2") << std::endl;
    std::cout << "index(item2)  = " << tv.index("item2")  << std::endl;

    // --- focus のテスト ---
    tv.focus("item1");
    std::cout << "focus = " << tv.focus() << std::endl;

    // --- tag_configure / tag_bind のテスト ---
    tv.tag_configure("highlight", {{"background", "yellow"}});
    tv.item("item1", {{"tags", "highlight"}});

    tv.tag_bind("highlight", "<Button-1>", [](const tk::Event& e){
        std::cout << "highlight tag clicked at (" << e.x << ", " << e.y << ")" << std::endl;
    });

    // --- identify_row / identify_column のテスト ---
    tv.bind("<Motion>", [&](const tk::Event& e){
        auto row = tv.identify_row(e.y);
        auto col = tv.identify_column(e.x);
        // std::cout << "hover row=" << row << " col=" << col << std::endl;
    });

    // --- bbox のテスト ---
    tv.after(300, [&](){
        auto box = tv.bbox("item1", "name");
        if (!box.empty())
        {
            std::cout << "bbox(item1,name) = "
                    << box[0] << ", "
                    << box[1] << ", "
                    << box[2] << ", "
                    << box[3] << std::endl;
        }
        else
        {
            std::cout << "bbox returned empty (not visible yet)" << std::endl;
        }
    });

    // --- Treeview を表示 ---
    tv.pack({{"fill", "both"}, {"expand", true}});

    root.mainloop();
    return 0;
}
