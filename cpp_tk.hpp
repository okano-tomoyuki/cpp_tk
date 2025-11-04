#ifndef CPP_TK_HPP
#define CPP_TK_HPP

#include <string>
#include <functional>
#include <map>
#include <unordered_map>
#include <vector>

namespace cpp_tk
{

static constexpr const char END[4]              = "end";        // 最後の位置
static constexpr const char INSERT[7]           = "insert";     // カーソル位置
static constexpr const char CURRENT[8]          = "current";    // マウスポインタ下の要素（Canvasなど）
static constexpr const char ANCHOR[7]           = "anchor";     // 選択の起点
static constexpr const char SEL_FIRST[10]       = "sel.first";  // 選択範囲の先頭
static constexpr const char SEL_LAST[9]         = "sel.last";   // 選択範囲の末尾
static constexpr const char ACTIVE[7]           = "active";     // アクティブな要素（Listboxなど）
static constexpr const char NONE[5]             = "none";       // 無効状態（stateなど）
static constexpr const char NORMAL[7]           = "normal";     // 通常状態
static constexpr const char DISABLED[9]         = "disabled";   // 無効状態
static constexpr const char READONLY[9]         = "readonly";   // 読み取り専用（Entryなど）

static constexpr const char N[2]                = "n";          // 北（上）
static constexpr const char S[2]                = "s";          // 南（下）
static constexpr const char E[2]                = "e";          // 東（右）
static constexpr const char W[2]                = "w";          // 西（左）

static constexpr const char NE[3]               = "ne";         // 北東（右上）
static constexpr const char NW[3]               = "nw";         // 北西（左上）
static constexpr const char SE[3]               = "se";         // 南東（右下）
static constexpr const char SW[3]               = "sw";         // 南西（左下）
static constexpr const char CENTER[7]           = "center";     // 中央

static constexpr const char LEFT[5]             = "left";       //
static constexpr const char RIGHT[6]            = "right";      //
static constexpr const char TOP[4]              = "top";        //
static constexpr const char BOTTOM[7]           = "bottom";     //

// for fill
static constexpr const char FILL_NONE[5]        = "none";
static constexpr const char FILL_X[2]           = "x";
static constexpr const char FILL_Y[2]           = "y";
static constexpr const char FILL_BOTH[5]        = "both";

// for expand
static constexpr const char EXPAND_TRUE[5]      = "true";
static constexpr const char EXPAND_FALSE[6]     = "false";

// for anchor
static constexpr const char ANCHOR_N[2]         = "n";
static constexpr const char ANCHOR_S[2]         = "s";
static constexpr const char ANCHOR_E[2]         = "e";
static constexpr const char ANCHOR_W[2]         = "w";
static constexpr const char ANCHOR_NE[3]        = "ne";
static constexpr const char ANCHOR_NW[3]        = "nw";
static constexpr const char ANCHOR_SE[3]        = "se";
static constexpr const char ANCHOR_SW[3]        = "sw";
static constexpr const char ANCHOR_CENTER[7]    = "center";

// for relief
static constexpr const char FLAT[5]             = "flat";
static constexpr const char RAISED[7]           = "raised";
static constexpr const char SUNKEN[7]           = "sunken";
static constexpr const char GROOVE[7]           = "groove";
static constexpr const char RIDGE[6]            = "ridge";

// for wrap
// static constexpr const char NONE[5]             = "none";
static constexpr const char CHAR[5]             = "char";
static constexpr const char WORD[5]             = "word";

// for orient
static constexpr const char HORIZONTAL[11]      = "horizontal";
static constexpr const char VERTICAL[9]         = "vertical";

struct Event
{
    int         x;
    int         y;
    int         x_root;
    int         y_root;
    std::string widget;
    std::string character;
    std::string keysym;
    int         keycode;
    std::string type;
};

class Interpreter
{

public:

    Interpreter();
    
    ~Interpreter();

    std::string evaluate(const std::string &command, bool* success = nullptr);
    
    void set_var(const std::string& name, const std::string& value);
    
    std::string get_var(const std::string& name);

    void trace_var(const std::string& name, std::function<void(const std::string&)> callback);

    void register_void_callback(const std::string& name, std::function<void()> callback);
    
    void register_double_callback(const std::string& name, std::function<void(const double&)> callback);
    
    void register_string_callback(const std::string& name, std::function<void(const std::string&)> callback);
    
    void register_event_callback(const std::string& name, std::function<void(const Event&)> callback);

private:
    std::unordered_map<std::string, std::function<void(const Event&)>>          event_callback_map_;

    std::unordered_map<std::string, std::function<void()>>                      void_callback_map_;

    std::unordered_map<std::string, std::function<void(const double&)>>         double_callback_map_;

    std::unordered_map<std::string, std::function<void(const std::string&)>>    string_callback_map_;

    class Impl;
    Impl* impl_;
};

class Object
{

public:
    const std::string id;

    Object();

private:

    static std::string next();

};

class StringVar : public Object
{

public:

    StringVar(Interpreter *interp);

    void set(const std::string &value);

    std::string get() const;

    void trace(std::function<void(const std::string&)> callback);

    const std::string& name() const;

private:

    Interpreter *interp_;

    std::string name_;

};

class Widget : public Object
{

public:

    Widget(Widget *parent, const std::string &type, const std::string& name="");

    const std::string& full_name() const;

    Widget& pack(const std::string &options = "");

    Widget& grid(const std::string &options = "");

    Widget& place(const std::string &options = "");

    Widget& config(const std::map<std::string, std::string> &option);

    Widget& bind(const std::string& event, std::function<void(const Event&)> callback);

    std::string after(const int& ms, std::function<void()> callback);

    void after_idle(std::function<void()> callback);

    void after_cancel(const std::string& id);

    void destroy();

protected:
    Interpreter *interp_;

    std::string full_name_;

    int         after_id_;       
};

class Tk : public Widget 
{

public:

    explicit Tk();

    Tk& title(const std::string& title);

    Tk& geometry(const std::string &size);

    Tk& protocol(const std::string& name, std::function<void()> handler);

    void mainloop();

    void quit();

};

class Frame : public Widget
{

public:

    explicit Frame(Widget *parent);

    Frame& width(const int &width);

    Frame& height(const int &height);

};

class Toplevel : public Widget
{

public:

    explicit Toplevel(Widget *interp);

    Toplevel& title(const std::string &title_text);

    Toplevel& geometry(const std::string &size);

    Toplevel& protocol(const std::string& name, std::function<void()> handler);

};

class Button : public Widget
{

public:
    explicit Button(Widget *parent);

    Button& text(const std::string& text);

    Button& command(std::function<void()> callback);

};

class Canvas : public Widget
{

public:

    explicit Canvas(Widget *widget);

    Canvas& itemconfig(const std::string& id_or_tag, const std::map<std::string, std::string>& options);

    std::string create_line(const int& x1, const int& y1, const int& x2, const int& y2, const std::map<std::string, std::string>& options = {});

    std::string create_oval(const int& x1, const int& y1, const int& x2, const int& y2, const std::map<std::string, std::string>& options = {});

    std::string create_rectangle(const int& x1, const int& y1, const int& x2, const int& y2, const std::map<std::string, std::string>& options = {});

    Canvas& coords(const std::string& id_or_tag, const std::vector<int>& coords);

    Canvas& erase(const std::string& id_or_tag);

    Canvas& width(const int &width);

    Canvas& height(const int &height);

};

class Entry : public Widget
{

public:

    explicit Entry(Widget *parent);

    Entry& textvariable(StringVar &var);

    Entry& state(const std::string& state);

    Entry& icursor(const std::string& index);

    Entry& insert(const std::string& index, const std::string& text);

    int index(const std::string& index = "insert") const;

    Entry& erase(const std::string& start, const std::string& end = "");

    Entry& set(const std::string& value);

    std::string get() const;

private:

    StringVar* text_var_;

};

class Label : public Widget
{

public:

    explicit Label(Widget *parent);

    Label& text(const std::string &text);
};

class Listbox : public Widget 
{

public:

    explicit Listbox(Widget* parent);

    Listbox& insert(int index, const std::string& item);

    Listbox& erase(int start, int end);

    std::vector<int> curselection() const;

    std::string get(int index) const;

    Listbox& yscrollcommand(const std::string& callback);

    Listbox& selectmode(const std::string& mode);

};

class Scale : public Widget 
{

public:

    explicit Scale(Widget* parent);

    Scale& from(double val);

    Scale& to(double val);

    Scale& orient(const std::string& dir);

    Scale& command(std::function<void(const double&)> callback);

};

class Scrollbar : public Widget 
{

public:

    explicit Scrollbar(Widget* parent);

    Scrollbar& orient(const std::string& dir);

    Scrollbar& command(std::function<void(const std::string&)> callback);

    Scrollbar& set(const std::string& args);

};

class Text : public Widget 
{

public:

    explicit Text(Widget* parent);

    Text& insert(const std::string& index, const std::string& text);

    std::string get(const std::string& start, const std::string& end = "end") const ;

    Text& erase(const std::string& start, const std::string& end = "end");

    Text& yscrollcommand(std::function<void(std::string)> callback);

    Text& yview(const std::string& args);

    Text& wrap(const std::string& mode);
};

namespace ttk
{

class Button : public Widget
{

public:

    explicit Button(Widget *parent);

    Button& text(const std::string& text);

    Button& command(std::function<void()> callback);
};

class Combobox : public Widget 
{

public:

    explicit Combobox(Widget* parent);

    Combobox& values(const std::vector<std::string>& items);

    Combobox& textvariable(const StringVar& var);
};

class Entry : public Widget
{

public:

    explicit Entry(Widget *parent);

    Entry& textvariable(StringVar &var);

    Entry& state(const std::string& state);

    Entry& icursor(const std::string& index);

    Entry& insert(const std::string& index, const std::string& text);

    int index(const std::string& index = "insert") const;

    Entry& erase(const std::string& start, const std::string& end = "");

    Entry& set(const std::string& value);

    std::string get() const;

private:
    
    StringVar* text_var_;

};

class Notebook : public Widget 
{

public:
    
    explicit Notebook(Widget* parent);

    Notebook& add_tab(Widget& child, const std::string& label);

    Notebook& select(int index);
};

class Label : public Widget
{

public:

    explicit Label(Widget *parent);

    Label& text(const std::string &text);
};

} // ttk

namespace filedialog
{

std::string askopenfile(const std::map<std::string, std::string>& options = {});

std::string asksaveasfilename(const std::map<std::string, std::string>& options = {});

std::string askdirectory(const std::map<std::string, std::string>& options = {});

} // filedialog

namespace messagebox 
{

std::string showinfo(const std::string& title, const std::string& message);

std::string showwarning(const std::string& title, const std::string& message);

std::string showerror(const std::string& title, const std::string& message);

std::string askquestion(const std::string& title, const std::string& message);

bool askyesno(const std::string& title, const std::string& message);

bool askokcancel(const std::string& title, const std::string& message);

bool askretrycancel(const std::string& title, const std::string& message);

} // messagebox

} // cpp_tk

#endif // CPP_TK_HPP