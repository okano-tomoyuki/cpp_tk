// custom::Calendar(月表示カレンダーWidget)のデモ。日付を選択すると下のラベルに反映される。
#include "cpp_tk.hpp"
#include "custom.hpp"

int main()
{
    namespace tk = cpp_tk;

    tk::Tk root;
    root.title("cpp_tk Calendar demo");

    tk::custom::Calendar cal(root);
    cal.pack({{"padx", 12}, {"pady", 12}});

    tk::Label selected(root);
    selected.pack({{"pady", 12}});

    auto selected_handle = selected.handle();
    cal.command([selected_handle](int year, int month, int day) {
        tk::Label(selected_handle).text(
            "Selected: " + std::to_string(year) + "-" + std::to_string(month) + "-" + std::to_string(day));
    });

    int y = 0, m = 0, d = 0;
    cal.get_date(y, m, d);
    selected.text("Selected: " + std::to_string(y) + "-" + std::to_string(m) + "-" + std::to_string(d));

    root.mainloop();
}
