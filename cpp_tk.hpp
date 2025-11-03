#ifndef CPP_TK_HPP
#define CPP_TK_HPP

#include <tk.h>
#include <tcl.h>
#include <cstring>
#include <string>
#include <sstream>
#include <iostream>
#include <functional>
#include <map>

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

    explicit Event()
        : x(0)
        , y(0)
        , x_root(0)
        , y_root(0)
        , keycode(0)
    {}
};


class Interpreter
{

public:
    Interpreter()
    {
        interp_ = Tcl_CreateInterp();
        Tcl_Init(interp_);
        Tk_Init(interp_);
    }

    ~Interpreter()
    {
        Tcl_DeleteInterp(interp_);
    }

    std::string evaluate(const std::string &command, bool* success = nullptr)
    {
        int code = Tcl_Eval(interp_, command.c_str());
        if (success)
        {
            *success = (code == TCL_OK);
        }
        return Tcl_GetStringResult(interp_);
    }

    void set_var(const std::string& name, const std::string& value);
    std::string get_var(const std::string& name);

    Tcl_Interp *get() const
    {
        return interp_;
    }

    void register_void_callback(const std::string& name, std::function<void()> callback)
    {
        void_callback_map_[name] = callback;

        Tcl_CreateCommand(interp_, name.c_str(), [](ClientData client_data, Tcl_Interp*, int argc, const char* argv[]) -> int {
            auto* self = static_cast<Interpreter*>(client_data);
            auto it = self->void_callback_map_.find(argv[0]);
            if (it != self->void_callback_map_.end()) 
            {
                it->second();
            }
            return TCL_OK;
        }, this, nullptr);        
    }

    void register_double_callback(const std::string& name, std::function<void(const double&)> callback)
    {
        double_callback_map_[name] = callback;

        Tcl_CreateCommand(interp_, name.c_str(), [](ClientData client_data, Tcl_Interp*, int argc, const char* argv[]) -> int {

            auto* self = static_cast<Interpreter*>(client_data);
            auto it = self->double_callback_map_.find(argv[0]);
            if (it != self->double_callback_map_.end()) 
            {
                it->second(safe_stod(argv[1]));
            }
            return TCL_OK;
        }, this, nullptr);        
    }    

    void register_string_callback(const std::string& name, std::function<void(const std::string&)> callback)
    {
        string_callback_map_[name] = callback;

        Tcl_CreateCommand(interp_, name.c_str(), [](ClientData client_data, Tcl_Interp*, int argc, const char* argv[]) -> int {
            auto* self = static_cast<Interpreter*>(client_data);
            auto it = self->string_callback_map_.find(argv[0]);
            if (it != self->string_callback_map_.end()) 
            {
                it->second(argv[1]);
            }
            return TCL_OK;
        }, this, nullptr);        
    } 

    void register_event_callback(const std::string& name, std::function<void(const Event&)> callback) 
    {
        event_callback_map_[name] = callback;

        Tcl_CreateCommand(interp_, name.c_str(), [](ClientData client_data, Tcl_Interp*, int argc, const char* argv[]) -> int {
            auto* self = static_cast<Interpreter*>(client_data);
            auto it = self->event_callback_map_.find(argv[0]);

            if (it != self->event_callback_map_.end()) 
            {
                Event e;
                e.x         = safe_stol(argv[1]);
                e.y         = safe_stol(argv[2]);
                e.x_root    = safe_stol(argv[3]);
                e.y_root    = safe_stol(argv[4]);
                e.widget    = argv[5];
                e.keysym    = argv[6];
                e.keycode   = safe_stol(argv[7]);
                e.character = argv[8];
                e.type      = argv[9];
                it->second(e);
            }

            return TCL_OK;

        }, this, nullptr);
    }

private:
    std::unordered_map<std::string, std::function<void(const Event&)>>          event_callback_map_;
    std::unordered_map<std::string, std::function<void()>>                      void_callback_map_;
    std::unordered_map<std::string, std::function<void(const double&)>>         double_callback_map_;
    std::unordered_map<std::string, std::function<void(const std::string&)>>    string_callback_map_;
    Tcl_Interp* interp_;

    static long safe_stol(const char* s)
    {
        if (!s || *s == '\0' || std::strcmp(s, "??") == 0) 
            return 0;

        char* endptr = nullptr;
        int ret = std::strtol(s, &endptr, 10);

        if (endptr == s || *endptr != '\0') 
            return 0;

        return ret;
    }

    static double safe_stod(const char* s)
    {
        if (!s || *s == '\0' || std::strcmp(s, "??") == 0) 
            return 0.0;

        char* endptr = nullptr;
        double ret = std::strtod(s, &endptr);

        if (endptr == s || *endptr != '\0') 
            return 0.0;

        return ret;
    }

};

std::unordered_map<std::thread::id, Interpreter*> interp_map;

class Object
{
public:
    const std::string id;
    Object()
        : id(next())
    {}

private:
    static std::string next()
    {
        static std::uint64_t count = 0;
        return std::to_string(count++);
    }
};

class Widget : public Object
{
public:

    Widget(Widget *parent, const std::string &type, const std::string& name="")
        : interp_(parent != nullptr ? parent->interp_ : new Interpreter())
        , after_id_(0)
    {
        if (parent != nullptr)
        {
            full_name_ = parent->full_name() + "." + (name.empty() ? type : name) + id;
            interp_->evaluate(type + " " + full_name_);
        }
        else
        {
            interp_map[std::this_thread::get_id()] = interp_;
            full_name_ = "";
        }
    }

    const std::string& full_name() const
    {
        return full_name_;
    }

    void pack(const std::string &options = "")
    {
        interp_->evaluate("pack " + full_name_ + " " + options);
    }

    void grid(const std::string &options = "")
    {
        interp_->evaluate("grid " + full_name_ + " " + options);
    }

    void place(const std::string &options = "")
    {
        interp_->evaluate("place " + full_name_ + " " + options);
    }

    void config(const std::map<std::string, std::string> &option)
    {
        std::ostringstream oss;
        oss << full_name() << " configure";
        for (const auto &kv : option)
        {
            oss << " -" << kv.first << " " << kv.second;
        }
        interp_->evaluate(oss.str());
    }

    void bind(const std::string& event, std::function<void(const Event&)> callback)
    {
        auto cb_name =  sanitize(full_name()) + "_" + sanitize(event) + "_bind_cb";
        interp_->register_event_callback(cb_name, callback);
        auto cmd = "bind " + full_name() + " " + event + " {" + cb_name + " %x %y %X %Y %W %K %k %c %t}";
        interp_->evaluate(cmd);
    }

    std::string after(const int& ms, std::function<void()> callback)
    {
        auto cb_name    = sanitize(full_name()) + "_after_cb_" + std::to_string(after_id_++);
        interp_->register_void_callback(cb_name, callback);
        auto cmd        = "after " + std::to_string(ms) + " " + cb_name;
        auto ok         = false;
        auto ret        = interp_->evaluate(cmd, &ok);

        if (!ok)
        {
            // Todo Error ハンドリング
        }

        return ret;
    }

    void after_idle(std::function<void()> callback)
    {
        auto cb_name    = sanitize(full_name()) + "_after_idle_cb";
        interp_->register_void_callback(cb_name, callback);
        auto cmd        = "after idle " + cb_name;
        auto ok         = false;
        auto ret        = interp_->evaluate(cmd, &ok);

        if (!ok)
        {
            // Todo Error ハンドリング
        }
    }

    void after_cancel(const std::string& id)
    {
        interp_->evaluate("after cancel " + id);
    }

    void destroy()
    {
        interp_->evaluate("destroy " + full_name());
    }

protected:
    Interpreter *interp_;
    std::string full_name_;
    int         after_id_;       

    static std::string sanitize(const std::string& s) 
    {
        std::string ret;
        for (char c : s) 
        {
            ret += std::isalnum(c) ? c : '_';
        }
        return ret;
    }
};

class Tk : public Widget 
{
public:
    Tk()
        : Widget(nullptr, "", "")
    {
        title("tk");
        geometry("300x300");
        protocol("WM_DELETE_WINDOW", [this](){quit();});
    }

    Tk& title(const std::string& title)
    {
        interp_->evaluate("wm title . \"" + title + "\"");
        return *this;
    }

    Tk& geometry(const std::string &size)
    {
        interp_->evaluate("wm geometry . " + size);
        return *this;
    }

    Tk& protocol(const std::string& name, std::function<void()> handler) 
    {
        auto cb_name = "protocol_cb_" + sanitize(name);
        interp_->register_void_callback(cb_name, handler);
        interp_->evaluate("wm protocol . " + name + " " + cb_name);
        return *this;
    }

    void mainloop() 
    {
        interp_->evaluate("vwait forever");
    }

    void quit() 
    {
        interp_->evaluate("set forever 1");
    }
};

class Frame : public Widget
{
public:
    Frame(Widget *parent)
        : Widget(parent, "frame", "f")
    {}

    Frame& width(const int &width)
    {
        config({{"width", std::to_string(width)}});
        return *this;
    }

    Frame& height(const int &height)
    {
        config({{"height", std::to_string(height)}});
        return *this;
    }
};

class Toplevel : public Widget
{
public:
    Toplevel(Widget *interp)
        : Widget(interp, "toplevel")
    {
        protocol("WM_DELETE_WINDOW", [this](){destroy();});
    }

    Toplevel& title(const std::string &title_text)
    {
        interp_->evaluate("wm title " + full_name() + " \"" + title_text + "\"");
        return *this;
    }

    Toplevel& geometry(const std::string &size)
    {
        interp_->evaluate("wm geometry " + full_name() + " " + size);
        return *this;
    }

    Toplevel& protocol(const std::string& name, std::function<void()> handler) 
    {
        std::string callback_name = "protocol_cb_" + sanitize(full_name()) + "_" + sanitize(name);
        interp_->register_void_callback(callback_name, handler);
        interp_->evaluate("wm protocol " + full_name() + " " + name + " " + callback_name);
        return *this;
    }
};

class Button : public Widget
{

public:
    Button(Widget *parent)
        : Widget(parent, "button", "b")
    {}

    Button& text(const std::string& text)
    {
        config({{"text", "\"" + text + "\""}});        
        return *this;
    }

    Button& command(std::function<void()> callback)
    {
        std::string callback_name =  sanitize(full_name()) + "_void_cb";
        interp_->register_void_callback(callback_name, callback);
        config({{"command", callback_name}});        
        return *this;
    }
};

class Canvas : public Widget
{
public:
    Canvas(Widget *widget)
        : Widget(widget, "canvas", "c")
    {}

    Canvas &create_line();

    Canvas &create_oval(const int &left, const int &up, const int &right, const int &down)
    {
        std::ostringstream oss;
        oss << full_name()
            << " " << "create"
            << " " << "oval"
            << " " << left
            << " " << up
            << " " << right
            << " " << down
            << " -fill red -outline green";
        interp_->evaluate(oss.str());
        return *this;
    }

    Canvas &create_rectangle(const int &left, const int &up, const int &right, const int &down)
    {
        std::ostringstream oss;
        oss << full_name() << " create" << " " << "rectangle" << " " << left << " " << up << " " << right << " " << down << " -fill red -outline green";
        interp_->evaluate(oss.str());
        return *this;
    }

    Canvas &config(const std::map<std::string, std::string> &option)
    {
        std::ostringstream oss;
        oss << full_name() << " configure";
        for (const auto &kv : option)
        {
            oss << " -" << kv.first << " " << kv.second;
        }
        interp_->evaluate(oss.str());
        return *this;
    }

    Canvas &width(const int &width)
    {
        return config({{"width", std::to_string(width)}});
    }

    Canvas &height(const int &height)
    {
        return config({{"height", std::to_string(height)}});
    }
};

class StringVar : public Object
{
public:
    StringVar(Interpreter *interp, const std::string &name = "")
        : interp_(interp)
    {
        name_ = name.empty() ? "var_" + id : name;
        Tcl_SetVar(interp_->get(), name_.c_str(), "", TCL_GLOBAL_ONLY);
    }

    void set(const std::string &value)
    {
        Tcl_SetVar(interp_->get(), name_.c_str(), value.c_str(), TCL_GLOBAL_ONLY);
    }

    std::string get() const
    {
        const char *val = Tcl_GetVar(interp_->get(), name_.c_str(), TCL_GLOBAL_ONLY);
        return val ? val : "";
    }

    const std::string& name() const 
    { 
        return name_; 
    }

private:
    Interpreter *interp_;
    std::string name_;
};

class Entry : public Widget
{
public:
    Entry(Widget *parent)
        : Widget(parent, "entry", "e") 
        , text_var_(nullptr)
    {}

    Entry& textvariable(StringVar &var)
    {
        text_var_ = &var;
        config({{"textvariable", var.name()}});
        return *this;
    }

    Entry& state(const std::string& state)
    {
        config({{"state", state}});
        return *this;
    }

    Entry& icursor(const std::string& index)
    {
        interp_->evaluate(full_name() + " icursor " + index);
        return *this;
    }

    Entry& insert(const std::string& index, const std::string& text) 
    {
        interp_->evaluate(full_name() + " insert " + index + " {" + text + "}");
        return *this;
    }

    int index(const std::string& index = "insert") const 
    {
        auto ok     = false;
        auto ret    = interp_->evaluate(full_name() + " index " + index, &ok);
        if (!ok) 
        {
            return -1;
        }
        return std::stol(ret);
    }

    Entry& erase(const std::string& start, const std::string& end = "") 
    {
        auto cmd = full_name() + " delete " + start;
        if (!end.empty()) 
        {
            cmd += " " + end;
        }
        interp_->evaluate(cmd);
        return *this;
    }

    Entry& set(const std::string& value) 
    {
        if (text_var_)
        {
            text_var_->set(value);
            return *this;
        }

        erase("0", "end");
        insert("0", value);
        return *this;
    }

    std::string get() const
    {
        if (text_var_)
        {
            return text_var_->get();
        }

        auto ok     = false;
        auto ret    = interp_->evaluate(full_name() + " get", &ok);
        if (!ok) 
        {
            // @todo エラーハンドリング
        }
        return ret;
    }

private:
    StringVar* text_var_;

};

class Label : public Widget
{
public:
    Label(Widget *parent)
        : Widget(parent, "label", "l") {}

    Label &text(const std::string &text)
    {
        config({{"text", "\"" +  text + "\""}});
        return *this;
    }
};

class Listbox : public Widget 
{

public:

    Listbox(Widget* parent)
        : Widget(parent, "listbox", "listbox") 
    {}

    Listbox& insert(int index, const std::string& item) 
    {
        interp_->evaluate(full_name() + " insert " + std::to_string(index) + " {" + item + "}");
        return *this;
    }

    Listbox& erase(int start, int end) 
    {
        interp_->evaluate(full_name() + " delete " + std::to_string(start) + " " + std::to_string(end));
        return *this;
    }

    std::vector<int> curselection() const 
    {
        std::string result = interp_->evaluate(full_name() + " curselection");
        return {}; //parse_indices(result); // "0 2 4" → {0, 2, 4}
    }

    std::string get(int index) const {
        return interp_->evaluate(full_name() + " get " + std::to_string(index));
    }

    Listbox& yscrollcommand(const std::string& callback) 
    {
        config({{"yscrollcommand", callback}});
        return *this;
    }

    Listbox& selectmode(const std::string& mode) 
    {
        config({{"selectmode", mode}}); // "single", "browse", "multiple", "extended"
        return *this;
    }
};


class Scale : public Widget 
{

public:
    Scale(Widget* parent)
        : Widget(parent, "scale", "scale") 
    {}
    
    Scale& from(double val) 
    { 
        config({{"from", std::to_string(val)}}); 
        return *this; 
    }

    Scale& to(double val)
    { 
        config({{"to", std::to_string(val)}}); 
        return *this; 
    }

    Scale& orient(const std::string& dir) 
    { 
        config({{"orient", dir}}); 
        return *this; 
    }

    Scale& command(std::function<void(const double&)> callback) 
    {
        std::string callback_name = sanitize(full_name()) + "_double_cb";
        interp_->register_double_callback(callback_name, callback);
        config({{"command", callback_name}});
        return *this;
    }
};

class Scrollbar : public Widget 
{
public:
    Scrollbar(Widget* parent) 
        : Widget(parent, "scrollbar") 
    {}

    Scrollbar& orient(const std::string& dir) 
    {
        config({{"orient", dir}});
        return *this;
    }

    Scrollbar& command(std::function<void(const std::string&)> callback) 
    {
        std::string callback_name = sanitize(full_name()) + "_scroll_cb";
        interp_->register_string_callback(callback_name, callback);
        config({{"command", callback_name}});
        return *this;
    }

    Scrollbar& set(const std::string& args) 
    {
        interp_->evaluate(full_name() + " set " + args);
        return *this;
    }

};

class Text : public Widget 
{

public:

    Text(Widget* parent) 
        : Widget(parent, "text", "text") 
    {}

    Text& insert(const std::string& index, const std::string& text) 
    {
        interp_->evaluate(full_name() + " insert " + index + " {" + text + "}");
        return *this;
    }

    std::string get(const std::string& start, const std::string& end = "end") const 
    {
        return interp_->evaluate(full_name() + " get " + start + " " + end);
    }

    Text& erase(const std::string& start, const std::string& end = "end") 
    {
        interp_->evaluate(full_name() + " delete " + start + " " + end);
        return *this;
    }

    Text& yscrollcommand(std::function<void(std::string)> callback) 
    {
        auto cb_name = sanitize(full_name()) + "_string_cb";
        interp_->register_string_callback(cb_name, callback);        
        config({{"yscrollcommand", cb_name}});
        return *this;
    }

    Text& yview(const std::string& args) 
    {
        interp_->evaluate(full_name() + " yview " + args);
        return *this;
    }

    Text& wrap(const std::string& mode) 
    {
        config({{"wrap", mode}});
        return *this;
    }
};

namespace ttk
{

class Button : public Widget
{

public:
    Button(Widget *parent)
        : Widget(parent, "ttk::button", "ttk_button")
    {}

    Button& text(const std::string& text)
    {
        config({{"text", "\"" + text + "\""}});        
        return *this;
    }

    Button& command(std::function<void()> callback)
    {
        std::string callback_name =  sanitize(full_name()) + "_void_callback";
        interp_->register_void_callback(callback_name, callback);
        config({{"command", callback_name}});        
        return *this;
    }
};

class Combobox : public Widget {
public:
    Combobox(Widget* parent) 
        : Widget(parent, "ttk::combobox", "ttk_combobox") 
    {}

    Combobox& values(const std::vector<std::string>& items) 
    {
        std::string list = "{";
        for (const auto& item : items) list += "{" + item + "} ";
        list += "}";
        config({{"values", list}});
        return *this;
    }

    Combobox& textvariable(const StringVar& var) 
    {
        config({{"textvariable", var.name()}});
        return *this;
    }
};

class Entry : public Widget
{
public:
    Entry(Widget *parent)
        : Widget(parent, "ttk::entry", "ttk_entry") 
        , text_var_(nullptr)
    {}

    Entry& textvariable(StringVar &var)
    {
        text_var_ = &var;
        config({{"textvariable", var.name()}});
        return *this;
    }

    Entry& state(const std::string& state)
    {
        config({{"state", state}});
        return *this;
    }

    Entry& icursor(const std::string& index)
    {
        interp_->evaluate(full_name() + " icursor " + index);
        return *this;
    }

    Entry& insert(const std::string& index, const std::string& text) 
    {
        interp_->evaluate(full_name() + " insert " + index + " {" + text + "}");
        return *this;
    }

    int index(const std::string& index = "insert") const 
    {
        auto ok     = false;
        auto ret    = interp_->evaluate(full_name() + " index " + index, &ok);
        if (!ok) 
        {
            return -1;
        }
        return std::stol(ret);
    }

    Entry& erase(const std::string& start, const std::string& end = "") 
    {
        auto cmd = full_name() + " delete " + start;
        if (!end.empty()) 
        {
            cmd += " " + end;
        }
        interp_->evaluate(cmd);
        return *this;
    }

    Entry& set(const std::string& value) 
    {
        if (text_var_)
        {
            text_var_->set(value);
            return *this;
        }

        erase("0", "end");
        insert("0", value);
        return *this;
    }

    std::string get() const
    {
        if (text_var_)
        {
            return text_var_->get();
        }

        auto ok     = false;
        auto ret    = interp_->evaluate(full_name() + " get", &ok);
        if (!ok) 
        {
            // @todo エラーハンドリング
        }
        return ret;
    }

private:
    StringVar* text_var_;

};

class Notebook : public Widget 
{
public:
    Notebook(Widget* parent) 
        : Widget(parent, "ttk::notebook", "ttk_notebook") 
    {}

    Notebook& add_tab(Widget& child, const std::string& label) 
    {
        interp_->evaluate(full_name() + " add " + child.full_name() + " -text {" + label + "}");
        return *this;
    }

    Notebook& select(int index) 
    {
        interp_->evaluate(full_name() + " select " + std::to_string(index));
        return *this;
    }
};

class Label : public Widget
{
public:
    Label(Widget *parent)
        : Widget(parent, "ttk::label", "tl") 
    {}

    Label& text(const std::string &text)
    {
        config({{"text", "\"" +  text + "\""}});
        return *this;
    }
};

} // ttk

namespace filedialog
{

static std::string askopenfile(const std::map<std::string, std::string>& options = {}) 
{
    std::string cmd = "tk_getOpenFile";
    for (const auto& kv : options) 
    {
        cmd += " -" + kv.first + " {" + kv.second + "}";
    }
    return interp_map[std::this_thread::get_id()]->evaluate(cmd);
}

static std::string asksaveasfilename(const std::map<std::string, std::string>& options = {}) 
{
    std::string cmd = "tk_getSaveFile";
    for (const auto& kv : options) 
    {
        cmd += " -" + kv.first + " {" + kv.second + "}";
    }
    return interp_map[std::this_thread::get_id()]->evaluate(cmd);
}

static std::string askdirectory(const std::map<std::string, std::string>& options = {}) 
{
    std::string cmd = "tk_chooseDirectory";
    for (const auto& kv : options) 
    {
        cmd += " -" + kv.first + " {" + kv.second + "}";
    }
    return interp_map[std::this_thread::get_id()]->evaluate(cmd);
}

} // filedialog

namespace messagebox 
{

std::string showinfo(const std::string& title, const std::string& message) 
{
    return interp_map[std::this_thread::get_id()]->evaluate("tk_messageBox -type ok -icon info -title {" + title + "} -message {" + message + "}");
}

std::string showwarning(const std::string& title, const std::string& message) 
{
    return interp_map[std::this_thread::get_id()]->evaluate("tk_messageBox -type ok -icon warning -title {" + title + "} -message {" + message + "}");
}

std::string showerror(const std::string& title, const std::string& message) 
{
    return interp_map[std::this_thread::get_id()]->evaluate("tk_messageBox -type ok -icon error -title {" + title + "} -message {" + message + "}");
}

std::string askquestion(const std::string& title, const std::string& message) 
{
    return interp_map[std::this_thread::get_id()]->evaluate("tk_messageBox -type yesno -icon question -title {" + title + "} -message {" + message + "}");
}

bool askyesno(const std::string& title, const std::string& message) 
{
    return askquestion(title, message) == "yes";
}

bool askokcancel(const std::string& title, const std::string& message) 
{
    std::string result = interp_map[std::this_thread::get_id()]->evaluate("tk_messageBox -type okcancel -icon question -title {" + title + "} -message {" + message + "}");
    return result == "ok";
}

bool askretrycancel(const std::string& title, const std::string& message) 
{
    std::string result = interp_map[std::this_thread::get_id()]->evaluate("tk_messageBox -type retrycancel -icon warning -title {" + title + "} -message {" + message + "}");
    return result == "retry";
}

} // messagebox

} // cpp_tk

#endif // CPP_TK_HPP