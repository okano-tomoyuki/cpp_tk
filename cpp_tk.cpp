#include <cstring>
#include <string>
#include <sstream>
#include <functional>
#include <map>
#include <unordered_map>
#include <thread>
#include <vector>
#include <iostream>

#include <tk.h>
#include <tcl.h>

#include "cpp_tk.hpp"

namespace 
{

long safe_stol(const char* s)
{
    if (!s || *s == '\0' || std::strcmp(s, "??") == 0) 
        return 0;

    char* endptr = nullptr;
    int ret = std::strtol(s, &endptr, 10);

    if (endptr == s || *endptr != '\0') 
        return 0;

    return ret;
}

double safe_stod(const char* s)
{
    if (!s || *s == '\0' || std::strcmp(s, "??") == 0) 
        return 0.0;

    char* endptr = nullptr;
    double ret = std::strtod(s, &endptr);

    if (endptr == s || *endptr != '\0') 
        return 0.0;

    return ret;
}

std::string sanitize(const std::string& s)
{
    std::string ret;
    for (char c : s) 
    {
        ret += std::isalnum(c) ? c : '_';
    }
    return ret;
}

}

namespace cpp_tk
{

std::unordered_map<std::thread::id, Interpreter*> interp_map;

class Interpreter
{

public:

    Interpreter()
        : interp_(nullptr)
    {
        interp_ = Tcl_CreateInterp();
        Tcl_Init(interp_);
        Tk_Init(interp_);
    }
    
    ~Interpreter()
    {
        Tcl_DeleteInterp(interp_);
        interp_ = nullptr;
    }

    std::string evaluate(const std::string &command, bool* success = nullptr)
    {
        int code = Tcl_Eval(interp_, command.c_str());
        if (success)
        {
            *success = (code == TCL_OK);
            
            if (!*success)
            {
                std::cerr << "Tcl Error: " << Tcl_GetStringResult(interp_) << std::endl;
            }
        }
        return Tcl_GetStringResult(interp_);
    }
    
    void set_var(const std::string& name, const std::string& value)
    {
        Tcl_SetVar(interp_, name.c_str(), value.c_str(), TCL_GLOBAL_ONLY);
    }
    
    std::string get_var(const std::string& name)
    {
        const char* val = Tcl_GetVar(interp_, name.c_str(), TCL_GLOBAL_ONLY);
        return val ? val : "";
    }

    void trace_var(const std::string& name, std::function<void(const std::string&)> callback)
    {
        string_callback_map_[name] = callback;
        Tcl_TraceVar(interp_, name.c_str(), TCL_TRACE_WRITES, [](ClientData client_data, Tcl_Interp* interp, const char* name1, const char* name2, int flags) -> char* {
            auto* self = static_cast<Interpreter*>(client_data);
            auto it = self->string_callback_map_.find(name1);
            if (it != self->string_callback_map_.end()) {
                const char* val = Tcl_GetVar(interp, name1, TCL_GLOBAL_ONLY);
                it->second(val ? val : "");
            }
            return nullptr;
        }, this);
    }

    void trace_var(const std::string& name, std::function<void(const int&)> callback)
    {
        int_callback_map_[name] = callback;
        Tcl_TraceVar(interp_, name.c_str(), TCL_TRACE_WRITES, [](ClientData client_data, Tcl_Interp* interp, const char* name1, const char* name2, int flags) -> char* {
            auto* self = static_cast<Interpreter*>(client_data);
            auto it = self->int_callback_map_.find(name1);
            if (it != self->int_callback_map_.end()) {
                const char* val = Tcl_GetVar(interp, name1, TCL_GLOBAL_ONLY);
                it->second(val ? std::stol(val) : 0);
            }
            return nullptr;
        }, this);
    }

    void trace_var(const std::string& name, std::function<void(const double&)> callback)
    {
        double_callback_map_[name] = callback;
        Tcl_TraceVar(interp_, name.c_str(), TCL_TRACE_WRITES, [](ClientData client_data, Tcl_Interp* interp, const char* name1, const char* name2, int flags) -> char* {
            auto* self = static_cast<Interpreter*>(client_data);
            auto it = self->double_callback_map_.find(name1);
            if (it != self->double_callback_map_.end()) {
                const char* val = Tcl_GetVar(interp, name1, TCL_GLOBAL_ONLY);
                it->second(val ? std::stod(val) : 0.0);
            }
            return nullptr;
        }, this);
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
                auto e      = Event();
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
    Tcl_Interp* interp_;

    std::unordered_map<std::string, std::function<void(const Event&)>>          event_callback_map_;

    std::unordered_map<std::string, std::function<void()>>                      void_callback_map_;

    std::unordered_map<std::string, std::function<void(const int&)>>            int_callback_map_;

    std::unordered_map<std::string, std::function<void(const double&)>>         double_callback_map_;

    std::unordered_map<std::string, std::function<void(const std::string&)>>    string_callback_map_;
};

Object::Object()
    : id(next())
{}

std::string Object::next()
{
    static int count = 0;
    return std::to_string(count++);
}

Var::Var(Widget* parent, const std::string& type)
    : interp_(parent->interp())
    , name_(type + "_var_" + id)
{}

const std::string& Var::name() const
{
    return name_;
}

std::string Var::get_var() const
{
    return interp_->get_var(name_);
}

void Var::set_var(const std::string& value)
{
    interp_->set_var(name_, value);
}

StringVar::StringVar(Widget* parent)
    : Var(parent, "string")
{
    interp_->set_var(name_, "");
}

void StringVar::set(const std::string &value)
{
    interp_->set_var(name_, value);
}

std::string StringVar::get() const
{
    return interp_->get_var(name_);
}

void StringVar::trace(std::function<void(const std::string&)> callback)
{
    interp_->trace_var(name_, callback);
}

BooleanVar::BooleanVar(Widget* parent)
    : Var(parent, "bool")
{
    interp_->set_var(name_, "0");
}

void BooleanVar::set(bool value)
{
    interp_->set_var(name_, value ? "1" : "0");
}

bool BooleanVar::get() const
{
    return interp_->get_var(name_) == "1";
}

void BooleanVar::trace(std::function<void(const bool&)> callback)
{
    interp_->trace_var(name_, [callback](const std::string& v){
        callback(v == "1");
    });
}

IntVar::IntVar(Widget* parent)
    : Var(parent, "int")
{
    interp_->set_var(name_, "0");
}

void IntVar::set(const int& value)
{
    interp_->set_var(name_, std::to_string(value));
}

int IntVar::get() const
{
    return std::stol(interp_->get_var(name_));
}

void IntVar::trace(std::function<void(const int&)> callback)
{
    interp_->trace_var(name_, callback);
}

DoubleVar::DoubleVar(Widget* parent)
    : Var(parent, "double")
{
    interp_->set_var(name_, "0.0");
}

void DoubleVar::set(const double& value)
{
    interp_->set_var(name_, std::to_string(value));
}

double DoubleVar::get() const
{
    return std::stod(interp_->get_var(name_));
}

void DoubleVar::trace(std::function<void(const double&)> callback)
{
    interp_->trace_var(name_, callback);
}

Widget::Widget(Widget *parent, const std::string &type, const std::string& name)
    : interp_(parent ? parent->interp_ : nullptr)
    , after_id_(0)
{
    if (parent != nullptr)
    {
        auto parent_name = (parent->full_name() == ".") ? "" : parent->full_name();
        full_name_ = parent_name + "." + (name.empty() ? type : name) + id;
        interp_->evaluate(type + " " + full_name_);
    }
    else
    {
        full_name_ = ".";
    }
}

const std::string& Widget::full_name() const
{
    return full_name_;
}

Widget& Widget::pack(const std::map<std::string, std::string> &options)
{
    std::ostringstream oss;
    oss << "pack " << full_name();
    for (const auto& kv : options)
    {
        oss << " -" << kv.first << " " << kv.second;
    }
    interp_->evaluate(oss.str());
    return *this;
}

Widget& Widget::grid(const std::map<std::string, std::string>& options)
{
    std::ostringstream oss;
    oss << "grid " << full_name();
    for (const auto& kv : options)
    {
        oss << " -" << kv.first << " " << kv.second;
    }
    interp_->evaluate(oss.str());
    return *this;
}

Widget& Widget::place(const std::map<std::string, std::string> &options)
{
    std::ostringstream oss;
    oss << "place " << full_name();
    for (const auto& kv : options)
    {
        oss << " -" << kv.first << " " << kv.second;
    }
    interp_->evaluate(oss.str());
    return *this;
}

Widget& Widget::config(const std::map<std::string, std::string> &option)
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

Widget& Widget::grid_rowconfigure(int row, const std::map<std::string, std::string>& options)
{
    std::ostringstream oss;
    oss << "grid rowconfigure " << full_name() << " " << row;
    for (const auto& kv : options)
    {
        oss << " -" << kv.first << " " << kv.second;
    }
    interp_->evaluate(oss.str());
    return *this;
}

Widget& Widget::grid_columnconfigure(int column, const std::map<std::string, std::string>& options)
{
    std::ostringstream oss;
    oss << "grid columnconfigure " << full_name() << " " << column;
    for (const auto& kv : options)
    {
        oss << " -" << kv.first << " " << kv.second;
    }
    interp_->evaluate(oss.str());
    return *this;
}

std::string Widget::cget(const std::string& name) const
{
    auto cmd = full_name() + " cget -" + name;
    auto ok  = false;
    auto ret = interp_->evaluate(cmd, &ok);

    if (!ok)
    {
        ret  = "";
        // Todo Error ハンドリング
    }

    return ret;
}

Widget& Widget::bind(const std::string& event, std::function<void(const Event&)> callback)
{
    auto cb_name =  sanitize(full_name()) + "_" + sanitize(event) + "_bind_cb";
    interp_->register_event_callback(cb_name, callback);
    auto cmd = "bind " + full_name() + " " + event + " {" + cb_name + " %x %y %X %Y %W %K %k %c %t}";
    interp_->evaluate(cmd);
    return *this;
}

std::string Widget::after(const int& ms, std::function<void()> callback)
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

void Widget::after_idle(std::function<void()> callback)
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

void Widget::after_cancel(const std::string& id)
{
    interp_->evaluate("after cancel " + id);
}

void Widget::destroy()
{
    interp_->evaluate("destroy " + full_name());
}

int Widget::winfo_width() const
{
    auto ret = interp_->evaluate("winfo width " + full_name());
    return safe_stol(ret.c_str());
}

int Widget::winfo_height() const
{
    auto ret = interp_->evaluate("winfo height " + full_name());
    return safe_stol(ret.c_str());
}

int Widget::winfo_x() const
{
    auto ret = interp_->evaluate("winfo x " + full_name());
    return safe_stol(ret.c_str());
}

int Widget::winfo_y() const
{
    auto ret = interp_->evaluate("winfo y " + full_name());
    return safe_stol(ret.c_str());
}

int Widget::winfo_rootx() const
{
    auto ret = interp_->evaluate("winfo rootx " + full_name());
    return safe_stol(ret.c_str());
}

int Widget::winfo_rooty() const
{
    auto ret = interp_->evaluate("winfo rooty " + full_name());
    return safe_stol(ret.c_str());
}

bool Widget::winfo_exists() const
{
    auto ret = interp_->evaluate("winfo exists " + full_name());
    return safe_stol(ret.c_str()) != 0;
}

std::string Widget::winfo_class() const
{
    return interp_->evaluate("winfo class " + full_name());
}

std::string Widget::winfo_toplevel() const
{
    return interp_->evaluate("winfo toplevel " + full_name());
}

std::vector<std::string> Widget::winfo_children() const
{
    auto result = interp_->evaluate("winfo children " + full_name());

    std::vector<std::string> children;
    std::istringstream iss(result);
    std::string child;

    while (iss >> child)
        children.push_back(child);

    return children;
}

Interpreter* Widget::interp()
{
    return interp_;
}

Tk::Tk()
    : Widget(nullptr, "", "")
{
    interp_ = new Interpreter();
    interp_map[std::this_thread::get_id()] = interp_;

    title("tk");
    geometry("300x300");
    protocol("WM_DELETE_WINDOW", [this](){quit();});
}

Tk& Tk::title(const std::string& title)
{
    interp_->evaluate("wm title . \"" + title + "\"");
    return *this;
}

Tk& Tk::geometry(const std::string &size)
{
    interp_->evaluate("wm geometry . " + size);
    return *this;
}

Tk& Tk::protocol(const std::string& name, std::function<void()> handler) 
{
    auto cb_name = "protocol_cb_" + sanitize(name);
    interp_->register_void_callback(cb_name, handler);
    interp_->evaluate("wm protocol . " + name + " " + cb_name);
    return *this;
}

void Tk::mainloop() 
{
    interp_->evaluate("vwait forever");
}

void Tk::quit() 
{
    interp_->evaluate("set forever 1");
}

Checkbutton::Checkbutton(Widget* parent)
    : Widget(parent, "checkbutton", "chk")
{}

Checkbutton& Checkbutton::text(const std::string& text)
{
    config({{"text", "\"" + text + "\""}});
    return *this;
}

Checkbutton& Checkbutton::variable(Var* var)
{
    config({{"variable", var->name()}});
    return *this;
}

Checkbutton& Checkbutton::command(std::function<void()> callback)
{
    auto cb = sanitize(full_name()) + "_chk_cb";
    interp_->register_void_callback(cb, callback);
    config({{"command", cb}});
    return *this;
}

Frame::Frame(Widget *parent)
    : Widget(parent, "frame", "f")
{}

Frame& Frame::width(const int &width)
{
    config({{"width", std::to_string(width)}});
    return *this;
}

Frame& Frame::height(const int &height)
{
    config({{"height", std::to_string(height)}});
    return *this;
}

Toplevel::Toplevel(Widget *interp)
    : Widget(interp, "toplevel")
{
    protocol("WM_DELETE_WINDOW", [this](){destroy();});
}

Toplevel& Toplevel::title(const std::string &title_text)
{
    interp_->evaluate("wm title " + full_name() + " \"" + title_text + "\"");
    return *this;
}

Toplevel& Toplevel::geometry(const std::string &size)
{
    interp_->evaluate("wm geometry " + full_name() + " " + size);
    return *this;
}

Toplevel& Toplevel::protocol(const std::string& name, std::function<void()> handler) 
{
    std::string callback_name = "protocol_cb_" + sanitize(full_name()) + "_" + sanitize(name);
    interp_->register_void_callback(callback_name, handler);
    interp_->evaluate("wm protocol " + full_name() + " " + name + " " + callback_name);
    return *this;
}

Button::Button(Widget *parent)
    : Widget(parent, "button", "b")
{}

Button& Button::width(const int& width)
{
    config({{"width",  std::to_string(width)}});
    return *this;
}

Button& Button::height(const int& height)
{
    config({{"height",  std::to_string(height)}});
    return *this;
}

Button& Button::text(const std::string& text)
{
    config({{"text", "\"" + text + "\""}});        
    return *this;
}

Button& Button::command(std::function<void()> callback)
{
    std::string callback_name =  sanitize(full_name()) + "_void_cb";
    interp_->register_void_callback(callback_name, callback);
    config({{"command", callback_name}});        
    return *this;
}

Canvas::Canvas(Widget *widget)
    : Widget(widget, "canvas", "c")
{}

Canvas& Canvas::itemconfig(const std::string& id_or_tag, const std::map<std::string, std::string>& options)
{
    std::ostringstream oss;
    oss << full_name() << " itemconfig " << id_or_tag;
    for (const auto &kv : options)
    {
        oss << " -" << kv.first << " " << kv.second;
    }
    interp_->evaluate(oss.str());
    return *this;
}

std::string Canvas::create_line(const int& x1, const int& y1, const int& x2, const int& y2, const std::map<std::string, std::string>& options)
{
    std::ostringstream oss;
    oss << full_name()
        << " " << "create"
        << " " << "line"
        << " " << x1
        << " " << y1
        << " " << x2
        << " " << y2;
    for (const auto &kv : options)
    {
        oss << " -" << kv.first << " " << kv.second;
    }
    return interp_->evaluate(oss.str());
}

std::string Canvas::create_oval(const int& x1, const int& y1, const int& x2, const int& y2, const std::map<std::string, std::string>& options)
{
    std::ostringstream oss;
    oss << full_name()
        << " " << "create"
        << " " << "oval"
        << " " << x1
        << " " << y1
        << " " << x2
        << " " << y2;
    for (const auto &kv : options)
    {
        oss << " -" << kv.first << " " << kv.second;
    }
    return interp_->evaluate(oss.str());
}

std::string Canvas::create_rectangle(const int& x1, const int& y1, const int& x2, const int& y2, const std::map<std::string, std::string>& options)
{
    std::ostringstream oss;
    oss << full_name()
        << " " << "create"
        << " " << "rectangle"
        << " " << x1
        << " " << y1
        << " " << x2
        << " " << y2;
    for (const auto &kv : options)
    {
        oss << " -" << kv.first << " " << kv.second;
    }
    return interp_->evaluate(oss.str());
}

std::string Canvas::create_text(const int& x, const int& y, const std::map<std::string, std::string>& options)
{
    std::ostringstream oss;
    oss << full_name()
        << " " << "create"
        << " " << "text"
        << " " << x
        << " " << y;
    for (const auto &kv : options)
    {
        oss << " -" << kv.first << " " << kv.second;
    }
    return interp_->evaluate(oss.str());
}

std::string Canvas::create_polygon(const std::vector<int>& coords, const std::map<std::string, std::string>& options)
{
    std::ostringstream oss;
    oss << full_name() << " create polygon";

    for (int c : coords)
        oss << " " << c;

    for (const auto& kv : options)
        oss << " -" << kv.first << " " << kv.second;

    return interp_->evaluate(oss.str());
}

std::string Canvas::create_arc(int x1, int y1, int x2, int y2, const std::map<std::string, std::string>& options)
{
    std::ostringstream oss;
    oss << full_name()
        << " create arc "
        << x1 << " " << y1 << " " << x2 << " " << y2;

    for (const auto& kv : options)
        oss << " -" << kv.first << " " << kv.second;

    return interp_->evaluate(oss.str());
}

std::string Canvas::create_image(int x, int y, const std::map<std::string, std::string>& options)
{
    std::ostringstream oss;
    oss << full_name()
        << " create image "
        << x << " " << y;

    for (const auto& kv : options)
        oss << " -" << kv.first << " " << kv.second;

    return interp_->evaluate(oss.str());
}

std::string Canvas::create_window(int x, int y, Widget* widget, const std::map<std::string, std::string>& options)
{
    std::ostringstream oss;
    oss << full_name()
        << " create window "
        << x << " " << y
        << " -window " << widget->full_name();

    for (const auto& kv : options)
        oss << " -" << kv.first << " " << kv.second;

    return interp_->evaluate(oss.str());
}

std::vector<std::string> Canvas::find_overlapping(int x1, int y1, int x2, int y2) const
{
    auto result = interp_->evaluate(
        full_name() + " find overlapping " +
        std::to_string(x1) + " " +
        std::to_string(y1) + " " +
        std::to_string(x2) + " " +
        std::to_string(y2)
    );

    std::vector<std::string> ids;
    std::istringstream iss(result);
    std::string id;
    while (iss >> id)
        ids.push_back(id);

    return ids;
}

std::vector<std::string> Canvas::find_closest(int x, int y) const
{
    auto result = interp_->evaluate(
        full_name() + " find closest " +
        std::to_string(x) + " " +
        std::to_string(y)
    );

    std::vector<std::string> ids;
    std::istringstream iss(result);
    std::string id;
    while (iss >> id)
        ids.push_back(id);

    return ids;
}

Canvas& Canvas::addtag(const std::string& tag, const std::string& where, const std::string& target)
{
    interp_->evaluate(full_name() + " addtag " + tag + " " + where + " " + target);
    return *this;
}

Canvas& Canvas::dtag(const std::string& tag, const std::string& target)
{
    interp_->evaluate(full_name() + " dtag " + target + " " + tag);
    return *this;
}

std::vector<std::string> Canvas::gettags(const std::string& id) const
{
    auto result = interp_->evaluate(full_name() + " gettags " + id);

    std::vector<std::string> tags;
    std::istringstream iss(result);
    std::string tag;
    while (iss >> tag)
        tags.push_back(tag);

    return tags;
}

Canvas& Canvas::coords(const std::string& item_id, const std::vector<int>& coords)
{
    std::ostringstream oss;
    oss << full_name() << " coords " << item_id;
    for (const auto& c : coords)
    {
        oss << " " << c;
    }
    interp_->evaluate(oss.str());
    return *this;
}

Canvas& Canvas::move(const std::string& id_or_tag, const int& x, const int& y)
{
    interp_->evaluate(full_name() + " move " + id_or_tag + " " + std::to_string(x) + " " + std::to_string(y));
    return *this;
}

Canvas& Canvas::moveto(const std::string& id_or_tag, const int& x, const int& y)
{
    interp_->evaluate(full_name() + " moveto " + id_or_tag + " " + std::to_string(x) + " " + std::to_string(y));
    return *this;
}

Canvas& Canvas::scale(const std::string& id_or_tag, const int& x, const int& y, const double& xscale, const double& yscale)
{
    interp_->evaluate(full_name() + " scale " + id_or_tag + " " + std::to_string(x) + " " + std::to_string(y) + " " + std::to_string(xscale) + " " + std::to_string(yscale));
    return *this;
}

Canvas& Canvas::rotate(const std::string& id_or_tag, const int& x, const int& y, const double& angle)
{
    interp_->evaluate(full_name() + " rotate " + id_or_tag + " " + std::to_string(x) + " " + std::to_string(y) + " " + std::to_string(angle));
    return *this;
}

Canvas& Canvas::erase(const std::string& id_or_tag)
{
    interp_->evaluate(full_name() + " delete " + id_or_tag);
    return *this;
}

Canvas& Canvas::width(const int &width)
{
    config({{"width", std::to_string(width)}});
    return *this;
}

Canvas& Canvas::height(const int &height)
{
    config({{"height", std::to_string(height)}});
    return *this;
}

Entry::Entry(Widget *parent)
    : Widget(parent, "entry", "e") 
    , text_var_(nullptr)
{}

Entry& Entry::textvariable(Var* var)
{
    text_var_ = var;
    config({{"textvariable", var->name()}});
    return *this;
}

Entry& Entry::state(const std::string& state)
{
    config({{"state", state}});
    return *this;
}

Entry& Entry::icursor(const std::string& index)
{
    interp_->evaluate(full_name() + " icursor " + index);
    return *this;
}

Entry& Entry::insert(const std::string& index, const std::string& text) 
{
    interp_->evaluate(full_name() + " insert " + index + " {" + text + "}");
    return *this;
}

int Entry::index(const std::string& index) const 
{
    auto ok     = false;
    auto ret    = interp_->evaluate(full_name() + " index " + index, &ok);
    if (!ok) 
    {
        return -1;
    }
    return std::stol(ret);
}

Entry& Entry::erase(const std::string& start, const std::string& end) 
{
    auto cmd = full_name() + " delete " + start;
    if (!end.empty()) 
    {
        cmd += " " + end;
    }
    interp_->evaluate(cmd);
    return *this;
}

Entry& Entry::set(const std::string& value) 
{
    if (text_var_)
    {
        text_var_->set_var(value);
        return *this;
    }

    erase("0", "end");
    insert("0", value);
    return *this;
}

std::string Entry::get() const
{
    if (text_var_)
    {
        return text_var_->get_var();
    }

    auto ok     = false;
    auto ret    = interp_->evaluate(full_name() + " get", &ok);
    if (!ok) 
    {
        // @todo エラーハンドリング
    }
    return ret;
}

Label::Label(Widget *parent)
    : Widget(parent, "label", "l") {}

Label& Label::text(const std::string &text)
{
    config({{"text", "\"" +  text + "\""}});
    return *this;
}

Listbox::Listbox(Widget* parent)
    : Widget(parent, "listbox", "listbox") 
{}

Listbox& Listbox::insert(int index, const std::string& item) 
{
    interp_->evaluate(full_name() + " insert " + std::to_string(index) + " {" + item + "}");
    return *this;
}

Listbox& Listbox::erase(int start, int end) 
{
    interp_->evaluate(full_name() + " delete " + std::to_string(start) + " " + std::to_string(end));
    return *this;
}

std::vector<int> Listbox::curselection() const 
{
    std::string result = interp_->evaluate(full_name() + " curselection");
    return {}; //parse_indices(result); // "0 2 4" → {0, 2, 4}
}

std::string Listbox::get(int index) const 
{
    return interp_->evaluate(full_name() + " get " + std::to_string(index));
}

Listbox& Listbox::yscrollcommand(const std::string& callback) 
{
    config({{"yscrollcommand", callback}});
    return *this;
}

Listbox& Listbox::selectmode(const std::string& mode) 
{
    config({{"selectmode", mode}}); // "single", "browse", "multiple", "extended"
    return *this;
}

Menu::Menu(Widget* parent, const std::map<std::string, std::string>& options)
    : Widget(parent, "menu", "menu")
{
    std::ostringstream oss;
    oss << full_name();
    for (const auto& kv : options)
    {
        oss << " -" << kv.first << " " << kv.second;
    }
    interp_->evaluate(oss.str());
}

Menu& Menu::add_command(const std::map<std::string, std::string>& options)
{
    std::ostringstream oss;
    oss << full_name() << " add command";
    for (const auto& kv : options)
    {
        oss << " -" << kv.first << " " << kv.second;
    }
    interp_->evaluate(oss.str());
    return *this;
}

Menu& Menu::add_cascade(const std::map<std::string, std::string>& options)
{
    std::ostringstream oss;
    oss << full_name() << " add cascade";
    for (const auto& kv : options)
    {
        oss << " -" << kv.first << " " << kv.second;
    }
    interp_->evaluate(oss.str());
    return *this;
}

Menu& Menu::add_separator()
{
    interp_->evaluate(full_name() + " add separator");
    return *this;
}

Menu& Menu::delete_item(const std::string& index)
{
    interp_->evaluate(full_name() + " delete " + index);
    return *this;
}

Menubutton::Menubutton(Widget* parent)
    : Widget(parent, "menubutton", "mb")
{}

Menubutton& Menubutton::menu(Menu* menu)
{
    config({{"menu", menu->full_name()}});
    return *this;
}

Message::Message(Widget* parent)
    : Widget(parent, "message", "msg")
{}

Message& Message::text(const std::string& text)
{
    config({{"text", "\"" + text + "\""}});
    return *this;
}

PanedWindow::PanedWindow(Widget* parent)
    : Widget(parent, "panedwindow", "pw")
{}

PanedWindow& PanedWindow::orient(const std::string& dir)
{
    config({{"orient", dir}});
    return *this;
}

PanedWindow& PanedWindow::add(Widget* child, const std::map<std::string, std::string>& options)
{
    std::ostringstream oss;
    oss << full_name() << " add " << child->full_name();
    for (const auto& kv : options)
    {
        oss << " -" << kv.first << " " << kv.second;
    }
    interp_->evaluate(oss.str());
    return *this;
}

PanedWindow& PanedWindow::forget(Widget* child)
{
    interp_->evaluate(full_name() + " forget " + child->full_name());
    return *this;
}

Radiobutton::Radiobutton(Widget* parent)
    : Widget(parent, "radiobutton", "rb")
{}

Radiobutton& Radiobutton::text(const std::string& text)
{
    config({{"text", "\"" + text + "\""}});
    return *this;
}

Radiobutton& Radiobutton::variable(Var* var)
{
    config({{"variable", var->name()}});
    return *this;
}

Radiobutton& Radiobutton::value(const std::string& val)
{
    config({{"value", "\"" + val + "\""}});
    return *this;
}

Radiobutton& Radiobutton::command(std::function<void()> callback)
{
    auto cb = sanitize(full_name()) + "_rb_cb";
    interp_->register_void_callback(cb, callback);
    config({{"command", cb}});
    return *this;
}

Scale::Scale(Widget* parent)
    : Widget(parent, "scale", "scale") 
{}
    
Scale& Scale::from(double val) 
{ 
    config({{"from", std::to_string(val)}}); 
    return *this; 
}

Scale& Scale::to(double val)
{ 
    config({{"to", std::to_string(val)}}); 
    return *this; 
}

Scale& Scale::orient(const std::string& dir) 
{ 
    config({{"orient", dir}}); 
    return *this; 
}

Scale& Scale::command(std::function<void(const double&)> callback) 
{
    std::string callback_name = sanitize(full_name()) + "_double_cb";
    interp_->register_double_callback(callback_name, callback);
    config({{"command", callback_name}});
    return *this;
}

Scrollbar::Scrollbar(Widget* parent) 
    : Widget(parent, "scrollbar") 
{}

Scrollbar& Scrollbar::orient(const std::string& dir) 
{
    config({{"orient", dir}});
    return *this;
}

Scrollbar& Scrollbar::command(std::function<void(const std::string&)> callback) 
{
    std::string callback_name = sanitize(full_name()) + "_scroll_cb";
    interp_->register_string_callback(callback_name, callback);
    config({{"command", callback_name}});
    return *this;
}

Scrollbar& Scrollbar::set(const std::string& args) 
{
    interp_->evaluate(full_name() + " set " + args);
    return *this;
}

Spinbox::Spinbox(Widget* parent)
    : Widget(parent, "spinbox", "sp")
{}

Spinbox& Spinbox::from(double val)
{
    config({{"from", std::to_string(val)}});
    return *this;
}

Spinbox& Spinbox::to(double val)
{
    config({{"to", std::to_string(val)}});
    return *this;
}

Spinbox& Spinbox::increment(double val)
{
    config({{"increment", std::to_string(val)}});
    return *this;
}

Spinbox& Spinbox::textvariable(Var* var)
{
    config({{"textvariable", var->name()}});
    return *this;
}

Spinbox& Spinbox::command(std::function<void()> callback)
{
    auto cb = sanitize(full_name()) + "_sp_cb";
    interp_->register_void_callback(cb, callback);
    config({{"command", cb}});
    return *this;
}

Text::Text(Widget* parent) 
    : Widget(parent, "text", "text") 
{}

Text& Text::insert(const std::string& index, const std::string& text) 
{
    interp_->evaluate(full_name() + " insert " + index + " {" + text + "}");
    return *this;
}

std::string Text::get(const std::string& start, const std::string& end) const 
{
    return interp_->evaluate(full_name() + " get " + start + " " + end);
}

Text& Text::erase(const std::string& start, const std::string& end) 
{
    interp_->evaluate(full_name() + " delete " + start + " " + end);
    return *this;
}

Text& Text::yscrollcommand(std::function<void(std::string)> callback) 
{
    auto cb_name = sanitize(full_name()) + "_string_cb";
    interp_->register_string_callback(cb_name, callback);        
    config({{"yscrollcommand", cb_name}});
    return *this;
}

Text& Text::yview(const std::string& args) 
{
    interp_->evaluate(full_name() + " yview " + args);
    return *this;
}

Text& Text::wrap(const std::string& mode) 
{
    config({{"wrap", mode}});
    return *this;
}

Text& Text::tag_add(const std::string& tag, const std::string& start, const std::string& end)
{
    interp_->evaluate(full_name() + " tag add " + tag + " " + start + " " + end);
    return *this;
}

Text& Text::tag_remove(const std::string& tag, const std::string& start, const std::string& end)
{
    interp_->evaluate(full_name() + " tag remove " + tag + " " + start + " " + end);
    return *this;
}

Text& Text::tag_config(const std::string& tag, const std::map<std::string, std::string>& options)
{
    std::ostringstream oss;
    oss << full_name() << " tag configure " << tag;
    for (const auto& kv : options)
    {
        oss << " -" << kv.first << " " << kv.second;
    }
    interp_->evaluate(oss.str());
    return *this;
}

Text& Text::mark_set(const std::string& mark, const std::string& index)
{
    interp_->evaluate(full_name() + " mark set " + mark + " " + index);
    return *this;
}

Text& Text::mark_unset(const std::string& mark)
{
    interp_->evaluate(full_name() + " mark unset " + mark);
    return *this;
}

std::string Text::search(const std::string& pattern, const std::string& index, const std::map<std::string, std::string>& options)
{
    std::ostringstream oss;
    oss << full_name() << " search";

    for (const auto& kv : options)
    {
        oss << " -" << kv.first << " " << kv.second;
    }

    oss << " {" << pattern << "} " << index;

    auto ok  = false;
    auto ret = interp_->evaluate(oss.str(), &ok);

    if (!ok)
    {
        return "";
    }

    return ret;
}

namespace ttk
{

Font::Font(Widget* parent, const std::map<std::string, std::string>& option)
    : name_("font_" + id)
    , interp_(parent->interp())
{
    interp_->evaluate("font create " + name_);
}

Font& Font::config(const std::map<std::string, std::string>& option)
{
    std::ostringstream oss;
    oss << "font configure " << name_;
    for (const auto &kv : option)
    {
        oss << " -" << kv.first << " " << kv.second;
    }
    interp_->evaluate(oss.str());
    return *this;
}

Font& Font::size(const int& size)
{
    config({{"size", std::to_string(size)}});
    return *this;
}

Font& Font::weight(const std::string& weight)
{
    config({{"weight", weight}});
    return *this;
}

Font& Font::family(const std::string& family)
{
    config({{"family",  "\"" + family + "\""}});
    return *this;
}

Font& Font::slant(const std::string& slant)
{
    config({{"slant", slant}});
    return *this;
}

Font& Font::underline(const int& underline)
{
    config({{"underline", std::to_string(underline)}});
    return *this;
}

Font& Font::overstrike(const int& overstrike)
{
    config({{"overstrike", std::to_string(overstrike)}});
    return *this;
}

const std::string& Font::name() const
{
    return name_;
}

Button::Button(Widget *parent)
    : Widget(parent, "ttk::button", "ttk_button")
{}

Button& Button::width(const int& width)
{
    config({{"width",  std::to_string(width)}});
    return *this;
}

Button& Button::height(const int& height)
{
    config({{"height",  std::to_string(height)}});
    return *this;
}

Button& Button::text(const std::string& text)
{
    config({{"text", "\"" + text + "\""}});        
    return *this;
}

Button& Button::command(std::function<void()> callback)
{
    std::string callback_name =  sanitize(full_name()) + "_void_callback";
    interp_->register_void_callback(callback_name, callback);
    config({{"command", callback_name}});        
    return *this;
}

Button& Button::font(const Font& font)
{
    config({{"font", font.name()}});
    return *this;    
}

Combobox::Combobox(Widget* parent) 
    : Widget(parent, "ttk::combobox", "ttk_combobox")
    , text_var_(nullptr)
{}

Combobox& Combobox::values(const std::vector<std::string>& items) 
{
    std::string list = "{";
    for (const auto& item : items) list += "{" + item + "} ";
    list += "}";
    config({{"values", list}});
    return *this;
}

Combobox& Combobox::textvariable(Var* var) 
{
    text_var_ = var;
    config({{"textvariable", var->name()}});
    return *this;
}

Combobox& Combobox::width(const int& width)
{
    config({{"width", std::to_string(width)}});
    return *this;
}

Combobox& Combobox::height(const int& height)
{
    config({{"height", std::to_string(height)}});
    return *this;
}

Combobox& Combobox::justify(const std::string& justify)
{
    config({{"justify", justify}});
    return *this;
}

Combobox& Combobox::state(const std::string& state)
{
    config({{"state", state}});
    return *this;
}

Combobox& Combobox::font(const Font& font)
{
    config({{"font", font.name()}});
    return *this;
}

Entry::Entry(Widget *parent)
    : Widget(parent, "ttk::entry", "ttk_entry") 
    , text_var_(nullptr)
{}

Entry& Entry::textvariable(Var* var)
{
    text_var_ = var;
    config({{"textvariable", var->name()}});
    return *this;
}

Entry& Entry::state(const std::string& state)
{
    config({{"state", state}});
    return *this;
}

Entry& Entry::icursor(const std::string& index)
{
    interp_->evaluate(full_name() + " icursor " + index);
    return *this;
}

Entry& Entry::insert(const std::string& index, const std::string& text) 
{
    interp_->evaluate(full_name() + " insert " + index + " {" + text + "}");
    return *this;
}

int Entry::index(const std::string& index) const 
{
    auto ok     = false;
    auto ret    = interp_->evaluate(full_name() + " index " + index, &ok);
    if (!ok) 
    {
        return -1;
    }
    return std::stol(ret);
}

Entry& Entry::erase(const std::string& start, const std::string& end) 
{
    auto cmd = full_name() + " delete " + start;
    if (!end.empty()) 
    {
        cmd += " " + end;
    }
    interp_->evaluate(cmd);
    return *this;
}

Entry& Entry::set(const std::string& value) 
{
    if (text_var_)
    {
        text_var_->set_var(value);
        return *this;
    }

    erase("0", "end");
    insert("0", value);
    return *this;
}

std::string Entry::get() const
{
    if (text_var_)
    {
        return text_var_->get_var();
    }

    auto ok     = false;
    auto ret    = interp_->evaluate(full_name() + " get", &ok);
    if (!ok) 
    {
        // @todo エラーハンドリング
    }
    return ret;
}

Label::Label(Widget *parent)
    : Widget(parent, "ttk::label", "tl") 
{}

Label& Label::text(const std::string &text)
{
    config({{"text", "\"" +  text + "\""}});
    return *this;
}

Label& Label::anchor(const std::string& anchor)
{
    config({{"anchor", anchor}});
    return *this;
}

Label& Label::relief(const std::string& relief)
{
    config({{"relief", relief}});
    return *this;
}

Label& Label::font(const Font& font)
{
    config({{"font", font.name()}});
    return *this;
}

Labelframe::Labelframe(Widget* parent)
    : Widget(parent, "ttk::labelframe", "ttk_labelframe")
{}

Labelframe& Labelframe::text(const std::string& text)
{
    config({{"text", "\"" + text + "\""}});
    return *this;
}

Notebook::Notebook(Widget* parent) 
    : Widget(parent, "ttk::notebook", "ttk_notebook") 
{}

Notebook& Notebook::add_tab(Widget& child, const std::string& label) 
{
    interp_->evaluate(full_name() + " add " + child.full_name() + " -text {" + label + "}");
    return *this;
}

Notebook& Notebook::select(int index) 
{
    interp_->evaluate(full_name() + " select " + std::to_string(index));
    return *this;
}

Progressbar::Progressbar(Widget* parent)
    : Widget(parent, "ttk::progressbar", "ttk_progress")
{}

Progressbar& Progressbar::mode(const std::string& mode)
{
    config({{"mode", mode}});   // "determinate" or "indeterminate"
    return *this;
}

Progressbar& Progressbar::value(double v)
{
    config({{"value", std::to_string(v)}});
    return *this;
}

Progressbar& Progressbar::start(int interval)
{
    interp_->evaluate(full_name() + " start " + std::to_string(interval));
    return *this;
}

Progressbar& Progressbar::stop()
{
    interp_->evaluate(full_name() + " stop");
    return *this;
}

Progressbar& Progressbar::step(double amount)
{
    interp_->evaluate(full_name() + " step " + std::to_string(amount));
    return *this;
}

Separator::Separator(Widget* parent)
    : Widget(parent, "ttk::separator", "ttk_sep") 
{}

Sizegrip::Sizegrip(Widget* parent)
    : Widget(parent, "ttk::sizegrip", "ttk_sizegrip")
{}

Treeview::Treeview(Widget* parent)
    : Widget(parent, "ttk::treeview", "ttk_tree")
{}

Treeview& Treeview::insert(const std::string& parent, const std::string& index, const std::string& iid, const std::map<std::string, std::string>& options)
{
    std::ostringstream oss;
    oss << full_name()
        << " insert "
        << parent << " "
        << index  << " "
        << " -id " << iid;

    for (const auto& kv : options)
        oss << " -" << kv.first << " " << kv.second;

    interp_->evaluate(oss.str());
    return *this;
}

Treeview& Treeview::erase(const std::string& iid)
{
    interp_->evaluate(full_name() + " delete " + iid);
    return *this;
}

Treeview& Treeview::item(const std::string& iid, const std::map<std::string, std::string>& options)
{
    std::ostringstream oss;
    oss << full_name() << " item " << iid;

    for (const auto& kv : options)
        oss << " -" << kv.first << " " << kv.second;

    interp_->evaluate(oss.str());
    return *this;
}

Treeview& Treeview::heading(const std::string& column, const std::map<std::string, std::string>& options)
{
    std::ostringstream oss;
    oss << full_name() << " heading " << column;

    for (const auto& kv : options)
        oss << " -" << kv.first << " " << kv.second;

    interp_->evaluate(oss.str());
    return *this;
}

Treeview& Treeview::column(const std::string& column, const std::map<std::string, std::string>& options)
{
    std::ostringstream oss;
    oss << full_name() << " column " << column;

    for (const auto& kv : options)
        oss << " -" << kv.first << " " << kv.second;

    interp_->evaluate(oss.str());
    return *this;
}

std::vector<std::string> Treeview::selection() const
{
    auto result = interp_->evaluate(full_name() + " selection");

    std::vector<std::string> ids;
    std::istringstream iss(result);
    std::string id;

    while (iss >> id)
        ids.push_back(id);

    return ids;
}

} // ttk

namespace colorchooser
{

std::string askcolor(const std::map<std::string, std::string>& options)
{
    // Interpreter を取得
    auto* interp = cpp_tk::interp_map[std::this_thread::get_id()];
    if (!interp)
        return "";

    std::ostringstream oss;
    oss << "tk_chooseColor";

    for (const auto& kv : options)
    {
        oss << " -" << kv.first << " " << kv.second;
    }

    bool ok = false;
    auto ret = interp->evaluate(oss.str(), &ok);

    if (!ok)
        return "";

    return ret;   // 例: "#ff0000" または ""（キャンセル時）
}

} // namespace colorchooser

namespace filedialog
{

std::string askopenfile(const std::map<std::string, std::string>& options) 
{
    std::string cmd = "tk_getOpenFile";
    for (const auto& kv : options) 
    {
        cmd += " -" + kv.first + " {" + kv.second + "}";
    }
    return interp_map[std::this_thread::get_id()]->evaluate(cmd);
}

std::string asksaveasfilename(const std::map<std::string, std::string>& options) 
{
    std::string cmd = "tk_getSaveFile";
    for (const auto& kv : options) 
    {
        cmd += " -" + kv.first + " {" + kv.second + "}";
    }
    return interp_map[std::this_thread::get_id()]->evaluate(cmd);
}

std::string askdirectory(const std::map<std::string, std::string>& options) 
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