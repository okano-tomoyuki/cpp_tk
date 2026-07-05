// Sun Valley ttk theme(第三者製、MITライセンス。thirdparty/sv_ttk/参照)を適用した見た目を
// 手軽に確認するためのデモ。「Toggle theme」ボタンでdark/lightを切り替えられる。
#include "cpp_tk.hpp"
#include "custom.hpp"

int main()
{
    namespace tk  = cpp_tk;
    namespace ttk = tk::ttk;

    tk::Tk root;
    root.title("cpp_tk x Sun Valley ttk theme");
    root.geometry("420x420");

    // 状態(現在dark/lightどちらか)。ボタンのコールバックから参照するため、
    // Widget破棄後もダングリングしないshared_ptrで保持する。
    auto dark = std::make_shared<bool>(true);
    tk::custom::use_sv_ttk_theme(*dark);

    ttk::Frame container(root, {{"padding", 16}});
    container.pack({{"fill", "both"}, {"expand", 1}});

    ttk::Label heading(container, {{"text", "cpp_tk x Sun Valley"}, {"font", "SunValleyTitleLargeFont"}});
    heading.pack({{"anchor", "w"}, {"pady", 12}});

    ttk::Entry entry(container);
    entry.insert("0", "Type something...");
    entry.pack({{"fill", "x"}, {"pady", 4}});

    ttk::Combobox combo(container);
    combo.values({"Option A", "Option B", "Option C"});
    combo.set("Option A");
    combo.pack({{"fill", "x"}, {"pady", 4}});

    ttk::Checkbutton check(container, {{"text", "Enable notifications"}});
    check.pack({{"anchor", "w"}, {"pady", 4}});

    tk::StringVar choice;
    choice.set("1");
    // containerと同じ型(ttk::Frame)を親として直接渡すと、Widgetのコピー禁止のために
    // コピーコンストラクタと曖昧になる(docs/tasks.md B節参照)ため、Widget&へ明示的に
    // アップキャストする。
    ttk::Frame radio_row(static_cast<const tk::Widget&>(container));
    radio_row.pack({{"fill", "x"}, {"pady", 4}});
    ttk::Radiobutton radio1(radio_row, {{"text", "Choice 1"}});
    radio1.variable(choice).value("1");
    radio1.pack({{"side", "left"}, {"padx", 12}});
    ttk::Radiobutton radio2(radio_row, {{"text", "Choice 2"}});
    radio2.variable(choice).value("2");
    radio2.pack({{"side", "left"}});

    ttk::Progressbar progress(container, {{"mode", "determinate"}});
    progress.value(65);
    progress.pack({{"fill", "x"}, {"pady", 12}});

    ttk::Scale scale(container, {{"from", 0}, {"to", 100}});
    scale.set(40);
    scale.pack({{"fill", "x"}, {"pady", 4}});

    ttk::Button accent_button(container, {{"text", "Primary action"}, {"style", "Accent.TButton"}});
    accent_button.pack({{"fill", "x"}, {"pady", 16}});

    ttk::Button toggle_button(container, {{"text", "Toggle theme (dark/light)"}});
    toggle_button.pack({{"fill", "x"}});

    toggle_button.command([dark]() {
        *dark = !*dark;
        tk::custom::use_sv_ttk_theme(*dark);
    });

    root.mainloop();
}
