#include <cpp_tk.hpp>

class ScrolledText : public cpp_tk::Widget
{
public:
    ScrolledText() = default;

    explicit ScrolledText(const cpp_tk::Widget& parent, const std::map<std::string, cpp_tk::ArgValue>& options = {});

    cpp_tk::Text& text();
    cpp_tk::Scrollbar& scrollbar();

private:
    cpp_tk::Text text_;
    cpp_tk::Scrollbar scrollbar_;
};

ScrolledText::ScrolledText(const cpp_tk::Widget& parent, const std::map<std::string, cpp_tk::ArgValue>& options)
    : cpp_tk::Widget(parent, "frame", "scrolledtext")
{
    // Text 本体（master(*this) を渡して body 内で遅延生成）
    text_ = cpp_tk::Text(*this, options);

    // Scrollbar
    scrollbar_ = cpp_tk::Scrollbar(*this, {{"orient", "vertical"}});

    // レイアウト
    text_.grid({{"row", 0}, {"column", 0}, {"sticky", "nsew"}});
    scrollbar_.grid({{"row", 0}, {"column", 1}, {"sticky", "ns"}});

    // Grid の伸縮設定
    grid_rowconfigure(0, {{"weight", 1}});
    grid_columnconfigure(0, {{"weight", 1}});

    // 双方向連携
    text_.yscrollcommand([this](std::string args){
        scrollbar_.set(args);
    });

    scrollbar_.command([this](const std::string& args){
        text_.yview(args);
    });
}

cpp_tk::Text& ScrolledText::text()
{
    return text_;
}

cpp_tk::Scrollbar& ScrolledText::scrollbar()
{
    return scrollbar_;
}

int main()
{
    auto root = cpp_tk::Tk();
    auto text = ScrolledText(root);
    text.pack();
    root.mainloop();
    return 0;
}
