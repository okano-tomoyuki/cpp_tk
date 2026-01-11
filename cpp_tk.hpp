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

class Interpreter;
class Widget;

class ArgValue 
{
public:
    enum class ValueType 
    {
        NONE,
        STRING,
        INT,
        DOUBLE,
        BOOL
    };

    ArgValue();
    
    ArgValue(const std::string& s);
    
    ArgValue(const char* s);
    
    ArgValue(int v);
    
    ArgValue(double v);
    
    ArgValue(bool v);

    ArgValue(const ArgValue& other);
    
    ArgValue& operator=(const ArgValue& other);

    ~ArgValue();

    ValueType type() const;

    std::string to_tcl() const;

private:
    ValueType type_;
    union 
    {
        int i_;
        double d_;
        bool b_;
    };
    std::string* str_;

    void cleanup();
    void copy_from(const ArgValue& other);
};

class Object
{

public:
    const std::string id;

    Object();

private:

    static std::string next();

};

class Var : public Object
{

public:
    explicit Var(Widget* parent, const std::string& type);

    virtual ~Var() = default;

    const std::string& name() const;

    std::string get_var() const;

    void set_var(const std::string& val);

protected:
    
    Interpreter* interp_;

    std::string  name_;

};

class StringVar : public Var
{

public:

    StringVar(Widget* parent);

    void set(const std::string &value);

    std::string get() const;

    void trace(std::function<void(const std::string&)> callback);

};

class BooleanVar : public Var 
{ 
public:

    explicit BooleanVar(Widget* parent); 

    void set(bool value); 

    bool get() const; 

    void trace(std::function<void(const bool&)> callback); 

};

class IntVar : public Var
{

public:

    IntVar(Widget* parent);

    void set(const int& value);

    int get() const;

    void trace(std::function<void(const int&)> callback);

};

class DoubleVar : public Var
{

public:

    DoubleVar(Widget* parent);

    void set(const double& value);

    double get() const;

    void trace(std::function<void(const double&)> callback);

};

class Widget : public Object
{

public:

    Widget(Widget* parent, const std::string &type, const std::string& name="");

    const std::string& full_name() const;

    Widget& pack(const std::map<std::string, std::string> &options = {});

    Widget& grid(const std::map<std::string, std::string> &options = {});

    Widget& place(const std::map<std::string, std::string> &options = {});

    Widget& config(const std::map<std::string, std::string> &option);

    Widget& grid_rowconfigure(int row, const std::map<std::string, std::string>& options);

    Widget& grid_columnconfigure(int column, const std::map<std::string, std::string>& options);

    std::string cget(const std::string& name) const;

    Widget& bind(const std::string& event, std::function<void(const Event&)> callback);

    std::string after(const int& ms, std::function<void()> callback);

    void after_idle(std::function<void()> callback);

    void after_cancel(const std::string& id);

    void destroy();

    int winfo_width() const;

    int winfo_height() const;

    int winfo_x() const;

    int winfo_y() const;

    int winfo_rootx() const;

    int winfo_rooty() const;

    bool winfo_exists() const;

    std::string winfo_class() const;

    std::string winfo_toplevel() const;

    std::vector<std::string> winfo_children() const;

    Interpreter* interp();

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

    std::string geometry() const;

    Tk& protocol(const std::string& name, std::function<void()> handler);

    Tk& resizable(bool width, bool height);

    Tk& minsize(int width, int height);

    Tk& maxsize(int width, int height);

    Tk& iconify(); 
    
    Tk& deiconify(); 
    
    Tk& withdraw(); 
    
    Tk& state(const std::string& new_state); 
    
    std::string state() const;

    Tk& attributes(const std::string& name, const std::string& value);

    std::string attributes(const std::string& name) const;

    Tk& lift();
    
    Tk& lower();

    Tk& grab_set();

    Tk& grab_release();

    Tk& iconphoto(const std::string& image_name);

    Tk& iconbitmap(const std::string& bitmap_path);

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

    std::string geometry() const;

    Toplevel& protocol(const std::string& name, std::function<void()> handler);

    Toplevel& resizable(bool width, bool height);

    Toplevel& minsize(int width, int height);

    Toplevel& maxsize(int width, int height);

    Toplevel& iconify();

    Toplevel& deiconify();

    Toplevel& withdraw();

    Toplevel& attributes(const std::string& name, const std::string& value);
    
    std::string attributes(const std::string& name) const;

    Toplevel& state(const std::string& new_state);

    std::string state() const;

    Toplevel& lift();
    
    Toplevel& lower();

    Toplevel& grab_set();

    Toplevel& grab_release();

    Toplevel& iconphoto(const std::string& image_name);

    Toplevel& iconbitmap(const std::string& bitmap_path);

};

class Button : public Widget
{

public:
    explicit Button(Widget *parent);

    Button& width(const int& width);

    Button& height(const int& height);

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

    std::string create_text(const int& x, const int& y, const std::map<std::string, std::string>& options = {});

    std::string create_polygon(const std::vector<int>& coords, const std::map<std::string, std::string>& options = {}); 
    
    std::string create_arc(int x1, int y1, int x2, int y2, const std::map<std::string, std::string>& options = {}); 
    
    std::string create_image(int x, int y, const std::map<std::string, std::string>& options = {}); 
    
    std::string create_window(int x, int y, Widget* widget, const std::map<std::string, std::string>& options = {}); 
    
    std::vector<std::string> find_overlapping(int x1, int y1, int x2, int y2) const; 
    
    std::vector<std::string> find_closest(int x, int y) const; 
    
    Canvas& addtag(const std::string& tag, const std::string& where, const std::string& target); 
    
    Canvas& dtag(const std::string& tag, const std::string& target); 
    
    std::vector<std::string> gettags(const std::string& id) const;

    Canvas& move(const std::string& id_or_tag, const int& x, const int& y);

    Canvas& moveto(const std::string& id_or_tag, const int& x, const int& y);

    Canvas& scale(const std::string& id_or_tag, const int& x, const int& y, const double& xscale, const double& yscale);

    Canvas& rotate(const std::string& id_or_tag, const int& x, const int& y, const double& angle);

    Canvas& coords(const std::string& id_or_tag, const std::vector<int>& coords);

    Canvas& erase(const std::string& id_or_tag);

    Canvas& width(const int &width);

    Canvas& height(const int &height);

};

class Checkbutton : public Widget 
{ 

public: 

    explicit Checkbutton(Widget* parent); 
    
    Checkbutton& text(const std::string& text); 
    
    Checkbutton& variable(Var* var); 
    
    Checkbutton& command(std::function<void()> callback); 

};

class Entry : public Widget
{

public:

    explicit Entry(Widget *parent);

    Entry& textvariable(Var* var);

    Entry& state(const std::string& state);

    Entry& icursor(const std::string& index);

    Entry& insert(const std::string& index, const std::string& text);

    int index(const std::string& index = "insert") const;

    Entry& erase(const std::string& start, const std::string& end = "");

    Entry& set(const std::string& value);

    std::string get() const;

private:

    Var* text_var_;

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

class Menu : public Widget 
{ 
public: 

    explicit Menu(Widget* parent, const std::map<std::string, std::string>& options = {}); 
    
    Menu& add_command(const std::map<std::string, std::string>& options); 
    
    Menu& add_cascade(const std::map<std::string, std::string>& options); 
    
    Menu& add_separator(); Menu& delete_item(const std::string& index); 

}; 

class Menubutton : public Widget 
{ 
    
public: 

    explicit Menubutton(Widget* parent); 
    
    Menubutton& menu(Menu* menu); 

};

class Message : public Widget 
{ 
    
public: 

    explicit Message(Widget* parent);
    
    Message& text(const std::string& text); 

};

class PanedWindow : public Widget 
{ 
    
public: 

    explicit PanedWindow(Widget* parent); 
    
    PanedWindow& orient(const std::string& dir); 
    
    PanedWindow& add(Widget* child, const std::map<std::string, std::string>& options = {}); 
    
    PanedWindow& forget(Widget* child); 

};

class Radiobutton : public Widget 
{ 
    
public: 

    explicit Radiobutton(Widget* parent); 
    
    Radiobutton& text(const std::string& text); 
    
    Radiobutton& variable(Var* var); 
    
    Radiobutton& value(const std::string& val); 
    
    Radiobutton& command(std::function<void()> callback); 

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

class Spinbox : public Widget 
{ 
    
public: 

    explicit Spinbox(Widget* parent); 
    
    Spinbox& from(double val); 
    
    Spinbox& to(double val); 
    
    Spinbox& increment(double val); 
    
    Spinbox& textvariable(Var* var); 
    
    Spinbox& command(std::function<void()> callback); 

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

    Text& tag_add(const std::string& tag, const std::string& start, const std::string& end); 

    Text& tag_remove(const std::string& tag, const std::string& start, const std::string& end); 

    Text& tag_config(const std::string& tag, const std::map<std::string, std::string>& options);

    Text& mark_set(const std::string& mark, const std::string& index); 
    
    Text& mark_unset(const std::string& mark);

    std::string search(const std::string& pattern, const std::string& index, const std::map<std::string, std::string>& options = {});

};

namespace ttk
{

class Font : public Object
{
public:

    explicit Font(Widget* parent, const std::map<std::string, std::string>& option = {});

    Font& config(const std::map<std::string, std::string>& option);

    Font& size(const int& size);

    Font& weight(const std::string& weight);

    Font& family(const std::string& family);

    Font& slant(const std::string& slant);

    Font& underline(const int& underline);

    Font& overstrike(const int& overstrike);

    const std::string& name() const;

private:

    Interpreter*    interp_;

    std::string     name_;

};

class Button : public Widget
{

public:

    explicit Button(Widget *parent);

    Button& width(const int& width);

    Button& height(const int& height);

    Button& text(const std::string& text);

    Button& command(std::function<void()> callback);

    Button& font(const Font& font);
};

class Combobox : public Widget 
{

public:

    explicit Combobox(Widget* parent);

    Combobox& values(const std::vector<std::string>& items);

    Combobox& textvariable(Var* var);

    Combobox& width(const int& width);

    Combobox& height(const int& height);

    Combobox& justify(const std::string& justify);

    Combobox& state(const std::string& state);

    Combobox& font(const Font& font);

private:

    Var* text_var_;
};

class Entry : public Widget
{

public:

    explicit Entry(Widget *parent);

    Entry& textvariable(Var* var);

    Entry& state(const std::string& state);

    Entry& icursor(const std::string& index);

    Entry& insert(const std::string& index, const std::string& text);

    int index(const std::string& index = "insert") const;

    Entry& erase(const std::string& start, const std::string& end = "");

    Entry& set(const std::string& value);

    std::string get() const;

    Entry& font(const Font& font);

private:
    
    Var* text_var_;

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

    Label& anchor(const std::string& anchor);

    Label& relief(const std::string& relief);

    Label& font(const Font& font);
};

class Labelframe : public Widget
{
public:
    explicit Labelframe(Widget* parent);

    Labelframe& text(const std::string& text);
};

class Progressbar : public Widget
{
public:
    explicit Progressbar(Widget* parent);

    Progressbar& mode(const std::string& mode);
    Progressbar& value(double v);
    Progressbar& start(int interval = 50);
    Progressbar& stop();
    Progressbar& step(double amount = 1.0);
};

class Separator : public Widget
{
public:
    explicit Separator(Widget* parent);
};


class Sizegrip : public Widget
{
public:
    explicit Sizegrip(Widget* parent);
};

class Treeview : public Widget
{
public:
    explicit Treeview(Widget* parent);

    Treeview& insert(const std::string& parent, const std::string& index, const std::string& iid, const std::map<std::string, std::string>& options = {});

    Treeview& erase(const std::string& iid);

    Treeview& item(const std::string& iid, const std::map<std::string, std::string>& options);

    Treeview& heading(const std::string& column, const std::map<std::string, std::string>& options);

    Treeview& column(const std::string& column, const std::map<std::string, std::string>& options);

    std::vector<std::string> selection() const;
};

} // ttk

namespace colorchooser
{
    std::string askcolor(const std::map<std::string, std::string>& options = {});
} // colorchooser

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