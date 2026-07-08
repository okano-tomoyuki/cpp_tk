#include "custom.hpp"
#include "thirdparty/sv_ttk/sv_ttk_data.hpp"

#include <cstdlib>
#include <ctime>

// custom.cppはcpp_tk.hppの公開APIだけを使って実装できる(Tcl_Interp*等のTcl/Tk内部には
// 一切触れない)。これはcore(cpp_tk.hpp/cpp_tk.cpp)が提供する薄いラッパーの上に、
// 合成ウィジェットが素直に組み上げられることの裏付けでもある。

namespace cpp_tk
{
namespace custom
{

// Calendarの日数/曜日計算はstd::mktime()に正規化させることで、閏年等のカレンダー演算を
// 自前で書かずに標準ライブラリへ委譲する(<ctime>のみでの実装、外部日付ライブラリ不要)。
namespace
{

int days_in_month(int year, int month)
{
    std::tm t = {};
    t.tm_year = year - 1900;
    t.tm_mon  = month; // 0始まりのtm_monに対し次月を指定し、day=0で「今月の最終日」に正規化させる
    t.tm_mday = 0;
    std::mktime(&t);
    return t.tm_mday;
}

int day_of_week(int year, int month, int day) // 0=日曜
{
    std::tm t = {};
    t.tm_year = year - 1900;
    t.tm_mon  = month - 1;
    t.tm_mday = day;
    t.tm_hour = 12; // 夏時間境界の影響を避けるため正午にしておく
    std::mktime(&t);
    return t.tm_wday;
}

const char* month_name(int month)
{
    static const char* names[] = {
        "January", "February", "March", "April", "May", "June",
        "July", "August", "September", "October", "November", "December"
    };
    return names[month - 1];
}

} // namespace

struct Calendar::Model
{
    int display_year  = 0;
    int display_month = 0; // 1-12
    int selected_year  = 0;
    int selected_month = 0;
    int selected_day   = 0;

    Label header;
    Frame grid;
    std::vector<Label> cells; // 表示月替わりのたびに作り直す(MVPとして単純さを優先)
    std::function<void(int, int, int)> on_select;
};

namespace
{

void rebuild_calendar_grid(const std::shared_ptr<Calendar::Model>& model);

void select_calendar_day(const std::shared_ptr<Calendar::Model>& model, int day)
{
    model->selected_year  = model->display_year;
    model->selected_month = model->display_month;
    model->selected_day   = day;
    rebuild_calendar_grid(model); // 選択ハイライトを反映する

    if (model->on_select)
        model->on_select(model->selected_year, model->selected_month, model->selected_day);
}

void go_calendar_month(const std::shared_ptr<Calendar::Model>& model, int delta)
{
    model->display_month += delta;
    if (model->display_month < 1)  { model->display_month = 12; --model->display_year; }
    if (model->display_month > 12) { model->display_month = 1;  ++model->display_year; }
    rebuild_calendar_grid(model);
}

void rebuild_calendar_grid(const std::shared_ptr<Calendar::Model>& model)
{
    for (auto& cell : model->cells) cell.destroy();
    model->cells.clear();

    model->header.text(std::string(month_name(model->display_month)) + " " + std::to_string(model->display_year));

    int first_wday  = day_of_week(model->display_year, model->display_month, 1);
    int total_days  = days_in_month(model->display_year, model->display_month);

    int row = 0;
    int col = first_wday;
    for (int d = 1; d <= total_days; ++d)
    {
        bool is_selected = (model->display_year  == model->selected_year &&
                             model->display_month == model->selected_month &&
                             d == model->selected_day);

        Label cell(model->grid.as_parent(), {{"text", std::to_string(d)}, {"width", 3}});
        if (is_selected)
            cell.config({{"background", "#2f60d8"}, {"foreground", "#ffffff"}});
        cell.grid({{"row", row}, {"column", col}, {"padx", 1}, {"pady", 1}});

        cell.bind("<Button-1>", [model, d](const Event&) { select_calendar_day(model, d); });

        model->cells.push_back(std::move(cell));
        if (++col > 6) { col = 0; ++row; }
    }
}

} // namespace

Calendar::Calendar(const Widget& parent, int year, int month, int day)
    : Widget(parent, "frame", "calendar")
    , model_(std::make_shared<Model>())
{
    if (year == 0 || month == 0 || day == 0)
    {
        std::time_t now = std::time(nullptr);
        std::tm* lt = std::localtime(&now);
        year  = lt->tm_year + 1900;
        month = lt->tm_mon + 1;
        day   = lt->tm_mday;
    }

    model_->display_year   = year;
    model_->display_month  = month;
    model_->selected_year   = year;
    model_->selected_month  = month;
    model_->selected_day    = day;

    Frame header_row(*this);
    header_row.grid({{"row", 0}, {"column", 0}, {"columnspan", 7}, {"sticky", "ew"}});

    Button prev(header_row, {{"text", "<"}});
    prev.grid({{"row", 0}, {"column", 0}});
    model_->header = Label(header_row, {{"anchor", "center"}});
    model_->header.grid({{"row", 0}, {"column", 1}});
    header_row.grid_columnconfigure(1, {{"weight", 1}});
    Button next(header_row, {{"text", ">"}});
    next.grid({{"row", 0}, {"column", 2}});

    auto model_for_callbacks = model_;
    prev.command([model_for_callbacks]() { go_calendar_month(model_for_callbacks, -1); });
    next.command([model_for_callbacks]() { go_calendar_month(model_for_callbacks, 1); });

    static const char* weekday_labels[] = {"Su", "Mo", "Tu", "We", "Th", "Fr", "Sa"};
    for (int i = 0; i < 7; ++i)
    {
        Label wd(*this, {{"text", weekday_labels[i]}, {"width", 3}});
        wd.grid({{"row", 1}, {"column", i}});
    }

    model_->grid = Frame(*this);
    model_->grid.grid({{"row", 2}, {"column", 0}, {"columnspan", 7}});

    rebuild_calendar_grid(model_);
}

void Calendar::get_date(int& year, int& month, int& day) const
{
    year  = model_->selected_year;
    month = model_->selected_month;
    day   = model_->selected_day;
}

Calendar& Calendar::set_date(int year, int month, int day)
{
    model_->display_year   = year;
    model_->display_month  = month;
    model_->selected_year   = year;
    model_->selected_month  = month;
    model_->selected_day    = day;
    rebuild_calendar_grid(model_);
    return *this;
}

Calendar& Calendar::command(std::function<void(int, int, int)> callback)
{
    model_->on_select = std::move(callback);
    return *this;
}

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

namespace simpledialog
{

std::string askstring(const Widget& parent, const std::string& title, const std::string& prompt,
                       const std::string& initial_value, bool* cancelled)
{
    Toplevel dialog(parent);
    dialog.title(title);
    dialog.transient(parent);
    dialog.resizable(false, false);

    Label label(dialog);
    label.text(prompt);
    label.pack({{"padx", 10}, {"pady", 10}});

    // StringVarはWidgetと異なりコピー禁止ではない(Tcl変数名を指すだけの軽量な値)ため、
    // handle()経由の再構築を使わずコールバックへそのまま値渡しできる。
    StringVar value;
    value.set(initial_value);

    Entry entry(dialog);
    entry.textvariable(value);
    entry.pack({{"padx", 10}, {"pady", 5}, {"fill", "x"}});
    entry.focus_set();

    auto ok_flag = std::make_shared<bool>(false);

    Frame button_row(dialog);
    button_row.pack({{"pady", 10}});

    // [this]や生ポインタの直接キャプチャはmove後にダングリングするため、handle()経由で
    // 再構築してから使う(Widget::handle()のコメント、docs/tasks.md B節参照)。
    auto dialog_handle = dialog.handle();

    Button ok_button(button_row);
    ok_button.text("OK");
    ok_button.command([dialog_handle, ok_flag]() {
        *ok_flag = true;
        Toplevel(dialog_handle).destroy();
    });
    ok_button.pack({{"side", "left"}, {"padx", 5}});

    Button cancel_button(button_row);
    cancel_button.text("Cancel");
    cancel_button.command([dialog_handle]() {
        Toplevel(dialog_handle).destroy();
    });
    cancel_button.pack({{"side", "left"}, {"padx", 5}});

    // 本家tkinter.simpledialogと同様にReturn/Escapeキーでも確定/キャンセルできるようにする。
    entry.bind("<Return>", [dialog_handle, ok_flag](const Event&) {
        *ok_flag = true;
        Toplevel(dialog_handle).destroy();
    });
    dialog.bind("<Escape>", [dialog_handle](const Event&) {
        Toplevel(dialog_handle).destroy();
    });

    dialog.protocol("WM_DELETE_WINDOW", [dialog_handle]() {
        Toplevel(dialog_handle).destroy();
    });

    dialog.grab_set();
    dialog.wait_window();

    if (cancelled) *cancelled = !*ok_flag;
    if (!*ok_flag) return "";
    return value.get();
}

int askinteger(const Widget& parent, const std::string& title, const std::string& prompt,
               int initial_value, bool* cancelled)
{
    bool dialog_cancelled = false;
    auto text = askstring(parent, title, prompt, std::to_string(initial_value), &dialog_cancelled);
    if (dialog_cancelled)
    {
        if (cancelled) *cancelled = true;
        return 0;
    }
    try
    {
        int value = std::stoi(text);
        if (cancelled) *cancelled = false;
        return value;
    }
    catch (...)
    {
        // 数値として解釈できない入力はキャンセル扱いにする(本家は再入力を促すが、
        // cpp_tkの簡略版ではその場でのバリデーションループまでは実装しない)。
        if (cancelled) *cancelled = true;
        return 0;
    }
}

double askfloat(const Widget& parent, const std::string& title, const std::string& prompt,
                 double initial_value, bool* cancelled)
{
    bool dialog_cancelled = false;
    auto text = askstring(parent, title, prompt, std::to_string(initial_value), &dialog_cancelled);
    if (dialog_cancelled)
    {
        if (cancelled) *cancelled = true;
        return 0.0;
    }
    try
    {
        double value = std::stod(text);
        if (cancelled) *cancelled = false;
        return value;
    }
    catch (...)
    {
        if (cancelled) *cancelled = true;
        return 0.0;
    }
}

} // simpledialog

void use_sv_ttk_theme(bool dark)
{
    ttk::Style style;

    if (style.call({"info", "exists", "::cpp_tk_sv_ttk_loaded"}) != "1")
    {
        const char* tmp = std::getenv("TEMP");
        if (!tmp) tmp = std::getenv("TMP");
        if (!tmp) tmp = std::getenv("TMPDIR");
        std::string base_dir = std::string(tmp ? tmp : "/tmp") + "/cpp_tk_sv_ttk";

        // 埋め込みアセットを書き出す共通プロシージャを1度だけ定義する。このスクリプト文字列は
        // cpp_tk自身が書いた固定文字列であり、埋め込みアセットの中身(content引数)は文字列展開
        // せず単なる引数値として渡すため、特殊文字によるインジェクションの懸念はない。
        style.call({"eval",
            "proc cpp_tk_sv_ttk_write {path content is_binary} {"
            "  file mkdir [file dirname $path];"
            "  if {$is_binary} {"
            "    set ch [open $path wb];"
            "    fconfigure $ch -translation binary;"
            "    puts -nonewline $ch [binary decode base64 $content];"
            "  } else {"
            "    set ch [open $path w];"
            "    puts -nonewline $ch $content;"
            "  };"
            "  close $ch"
            "}"
        });

        for (std::size_t i = 0; i < detail::sv_ttk_files_count; ++i)
        {
            const detail::EmbeddedFile& f = detail::sv_ttk_files[i];
            std::string full_path = base_dir + "/" + f.relative_path;
            std::string content(f.content, f.length);
            style.call({"cpp_tk_sv_ttk_write", full_path, content, f.binary_base64 ? "1" : "0"});
        }

        style.call({"source", base_dir + "/sv.tcl"});
        style.call({"set", "::cpp_tk_sv_ttk_loaded", "1"});
    }

    style.theme_use(dark ? "sun-valley-dark" : "sun-valley-light");

    // ttk::style theme use(内部で呼ぶttk::setTheme)は<<ThemeChanged>>を自動発火しないため、
    // sv.tclが登録したconfigure_colors等のバインド(配色の実適用)を確定させるには明示的に
    // 発火させる必要がある(本家のPython sv_ttkパッケージも同様に明示発火している)。
    // event generateによる発火は初回呼び出し直後だと反映されないことがあったため(要因未特定)、
    // "."自身の配色を決めるconfigure_colorsは直接呼び出して確実に適用する。event generateは
    // 既存のEntry/Combobox/Spinbox/Menu等(configure_colors以外の<<ThemeChanged>>バインド先)への
    // 反映のためあわせて発火しておく。
    style.call({"eval", "configure_colors"});
    style.call({"event", "generate", ".", "<<ThemeChanged>>"});
}

} // custom
} // cpp_tk
