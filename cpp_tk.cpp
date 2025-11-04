#include <cstring>
#include <string>
#include <sstream>
#include <functional>
#include <map>
#include <unordered_map>
#include <thread>
#include <vector>

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

class Interpreter::Impl
{
public:
    Tcl_Interp* interp;

    Impl()
        : interp(nullptr)
    {}
};

Interpreter::Interpreter()
    : impl_(new Impl())
{
    impl_->interp = Tcl_CreateInterp();
    Tcl_Init(impl_->interp);
    Tk_Init(impl_->interp);
}

Interpreter::~Interpreter()
{
    Tcl_DeleteInterp(impl_->interp);
    delete impl_;
}

std::string Interpreter::evaluate(const std::string &command, bool* success)
{
    int code = Tcl_Eval(impl_->interp, command.c_str());
    if (success)
    {
        *success = (code == TCL_OK);
    }
    return Tcl_GetStringResult(impl_->interp);
}

void Interpreter::set_var(const std::string& name, const std::string& value)
{
    Tcl_SetVar(impl_->interp, name.c_str(), value.c_str(), TCL_GLOBAL_ONLY);
}

std::string Interpreter::get_var(const std::string& name)
{
    const char* val = Tcl_GetVar(impl_->interp, name.c_str(), TCL_GLOBAL_ONLY);
    return val ? val : "";
}

void Interpreter::trace_var(const std::string& name, std::function<void(const std::string&)> callback)
{
    string_callback_map_[name] = callback;
    Tcl_TraceVar(impl_->interp, name.c_str(), TCL_TRACE_WRITES, [](ClientData client_data, Tcl_Interp* interp, const char* name1, const char* name2, int flags) -> char* {
        auto* self = static_cast<Interpreter*>(client_data);
        auto it = self->string_callback_map_.find(name1);
        if (it != self->string_callback_map_.end()) {
            const char* val = Tcl_GetVar(interp, name1, TCL_GLOBAL_ONLY);
            it->second(val ? val : "");
        }
        return nullptr;
    }, this);
}

void Interpreter::register_void_callback(const std::string& name, std::function<void()> callback)
{
    void_callback_map_[name] = callback;
    Tcl_CreateCommand(impl_->interp, name.c_str(), [](ClientData client_data, Tcl_Interp*, int argc, const char* argv[]) -> int {
        auto* self = static_cast<Interpreter*>(client_data);
        auto it = self->void_callback_map_.find(argv[0]);
        if (it != self->void_callback_map_.end()) 
        {
            it->second();
        }
        return TCL_OK;
    }, this, nullptr);        
}

void Interpreter::register_double_callback(const std::string& name, std::function<void(const double&)> callback)
{
    double_callback_map_[name] = callback;
    Tcl_CreateCommand(impl_->interp, name.c_str(), [](ClientData client_data, Tcl_Interp*, int argc, const char* argv[]) -> int {
        auto* self = static_cast<Interpreter*>(client_data);
        auto it = self->double_callback_map_.find(argv[0]);
        if (it != self->double_callback_map_.end()) 
        {
            it->second(safe_stod(argv[1]));
        }
        return TCL_OK;
    }, this, nullptr);        
}    

void Interpreter::register_string_callback(const std::string& name, std::function<void(const std::string&)> callback)
{
    string_callback_map_[name] = callback;
    Tcl_CreateCommand(impl_->interp, name.c_str(), [](ClientData client_data, Tcl_Interp*, int argc, const char* argv[]) -> int {
        auto* self = static_cast<Interpreter*>(client_data);
        auto it = self->string_callback_map_.find(argv[0]);
        if (it != self->string_callback_map_.end()) 
        {
            it->second(argv[1]);
        }
        return TCL_OK;
    }, this, nullptr);        
} 

void Interpreter::register_event_callback(const std::string& name, std::function<void(const Event&)> callback) 
{
    event_callback_map_[name] = callback;
    Tcl_CreateCommand(impl_->interp, name.c_str(), [](ClientData client_data, Tcl_Interp*, int argc, const char* argv[]) -> int {
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

Object::Object()
    : id(next())
{}

std::string Object::next()
{
    static int count = 0;
    return std::to_string(count++);
}

StringVar::StringVar(Interpreter *interp)
    : interp_(interp)
    , name_(name_ = "string_var_" + id)
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

const std::string& StringVar::name() const 
{ 
    return name_; 
}

Widget::Widget(Widget *parent, const std::string &type, const std::string& name)
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

const std::string& Widget::full_name() const
{
    return full_name_;
}

Widget& Widget::pack(const std::string &options)
{
    interp_->evaluate("pack " + full_name_ + " " + options);
    return *this;
}

Widget& Widget::grid(const std::string &options)
{
    interp_->evaluate("grid " + full_name_ + " " + options);
    return *this;
}

Widget& Widget::place(const std::string &options)
{
    interp_->evaluate("place " + full_name_ + " " + options);
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

Tk::Tk()
    : Widget(nullptr, "", "")
{
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

Entry& Entry::textvariable(StringVar &var)
{
    text_var_ = &var;
    config({{"textvariable", var.name()}});
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
        text_var_->set(value);
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

namespace ttk
{

Button::Button(Widget *parent)
    : Widget(parent, "ttk::button", "ttk_button")
{}

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

Combobox::Combobox(Widget* parent) 
    : Widget(parent, "ttk::combobox", "ttk_combobox") 
{}

Combobox& Combobox::values(const std::vector<std::string>& items) 
{
    std::string list = "{";
    for (const auto& item : items) list += "{" + item + "} ";
    list += "}";
    config({{"values", list}});
    return *this;
}

Combobox& Combobox::textvariable(const StringVar& var) 
{
    config({{"textvariable", var.name()}});
    return *this;
}

Entry::Entry(Widget *parent)
    : Widget(parent, "ttk::entry", "ttk_entry") 
    , text_var_(nullptr)
{}

Entry& Entry::textvariable(StringVar &var)
{
    text_var_ = &var;
    config({{"textvariable", var.name()}});
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
        text_var_->set(value);
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

Label::Label(Widget *parent)
    : Widget(parent, "ttk::label", "tl") 
{}

Label& Label::text(const std::string &text)
{
    config({{"text", "\"" +  text + "\""}});
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

} // ttk

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