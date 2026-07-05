#include "custom.hpp"

// custom.cppはcpp_tk.hppの公開APIだけを使って実装できる(Tcl_Interp*等のTcl/Tk内部には
// 一切触れない)。これはcore(cpp_tk.hpp/cpp_tk.cpp)が提供する薄いラッパーの上に、
// 合成ウィジェットが素直に組み上げられることの裏付けでもある。

namespace cpp_tk
{
namespace custom
{

ScrolledText::ScrolledText(const Widget& parent, const std::map<std::string, ArgValue>& options)
    : Widget(parent, "frame", "scrolledtext")
{
    text_      = Text(*this, options);
    scrollbar_ = Scrollbar(*this, {{"orient", "vertical"}});

    text_.grid({{"row", 0}, {"column", 0}, {"sticky", "nsew"}});
    scrollbar_.grid({{"row", 0}, {"column", 1}, {"sticky", "ns"}});

    grid_rowconfigure(0, {{"weight", 1}});
    grid_columnconfigure(0, {{"weight", 1}});

    // [this]や生ポインタの直接キャプチャはmove後にダングリングするため、handle()経由で
    // 再構築してから使う(Widget::handle()のコメント、docs/tasks.md B節参照)。
    auto scrollbar_handle = scrollbar_.handle();
    text_.yscrollcommand([scrollbar_handle](const std::string& args) {
        Scrollbar(scrollbar_handle).set(args);
    });

    auto text_handle = text_.handle();
    scrollbar_.command([text_handle](const std::string& args) {
        Text(text_handle).yview(args);
    });
}

Text& ScrolledText::text()
{
    return text_;
}

Scrollbar& ScrolledText::scrollbar()
{
    return scrollbar_;
}

// LabeledScaleの値表示用。std::to_stringは小数点以下6桁固定になるため、末尾の0(と余った
// 小数点)を削って見た目を整える。
static std::string format_labeled_scale_value(double value)
{
    std::string s = std::to_string(value);
    if (s.find('.') != std::string::npos)
    {
        while (!s.empty() && s.back() == '0') s.pop_back();
        if (!s.empty() && s.back() == '.') s.pop_back();
    }
    return s;
}

LabeledScale::LabeledScale(const Widget& parent, DoubleVar& variable, double from, double to)
    : Widget(parent, "frame", "labeledscale")
{
    label_ = ttk::Label(*this);
    scale_ = ttk::Scale(*this, {{"from", from}, {"to", to}, {"variable", variable}, {"orient", "horizontal"}});

    label_.pack({{"side", "top"}});
    scale_.pack({{"side", "top"}, {"fill", "x"}, {"expand", "true"}});

    label_.text(format_labeled_scale_value(variable.get()));

    // [this]や生ポインタの直接キャプチャはmove後にダングリングするため、handle()経由で
    // 再構築してから使う(Widget::handle()のコメント、docs/tasks.md B節参照)。
    auto label_handle = label_.handle();
    variable.trace([label_handle](const double& value) {
        ttk::Label(label_handle).text(format_labeled_scale_value(value));
    });
}

ttk::Scale& LabeledScale::scale()
{
    return scale_;
}

ttk::Label& LabeledScale::label()
{
    return label_;
}

} // custom
} // cpp_tk
