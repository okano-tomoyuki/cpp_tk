#include <cstring>
#include <string>
#include <sstream>
#include <functional>
#include <map>
#include <unordered_map>
#include <thread>
#include <vector>
#include <iostream>
#include <array>
#include <cstdint>


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

// ArgValue を対応する Tcl_Obj* に変換する内部ヘルパー
Tcl_Obj* make_obj(Tcl_Interp* interp, const cpp_tk::ArgValue& v)
{
    switch (v.type())
    {
        case cpp_tk::ArgValue::ValueType::STRING:
            return Tcl_NewStringObj(v.as_string().c_str(), (int)v.as_string().size());
        case cpp_tk::ArgValue::ValueType::INT:
            return Tcl_NewIntObj(v.as_int());
        case cpp_tk::ArgValue::ValueType::DOUBLE:
            return Tcl_NewDoubleObj(v.as_double());
        case cpp_tk::ArgValue::ValueType::BOOL:
            return Tcl_NewBooleanObj(v.as_bool() ? 1 : 0);
        case cpp_tk::ArgValue::ValueType::BYTES:
            return Tcl_NewByteArrayObj(v.as_bytes().data(), (int)v.as_bytes().size());
        default:
            return Tcl_NewObj();
    }
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

    // ArgValue のリストをそのまま渡してコマンドを実行する。
    // Tcl_Obj への変換は内部で行うため、呼び出し側は TCL API に依存しない。
    std::string invoke(const std::vector<ArgValue>& words, bool* success = nullptr)
    {
        std::vector<Tcl_Obj*> objv;
        objv.reserve(words.size());
        for (const auto& w : words)
            objv.push_back(make_obj(interp_, w));

        for (auto* obj : objv)
            Tcl_IncrRefCount(obj);

        int code = Tcl_EvalObjv(interp_, (int)objv.size(), objv.data(), 0);

        for (auto* obj : objv)
            Tcl_DecrRefCount(obj);

        if (success)
        {
            *success = (code == TCL_OK);
            if (!*success)
                std::cerr << "Tcl Error: " << Tcl_GetStringResult(interp_) << std::endl;
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
                e.delta     = safe_stol(argv[10]);
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

ArgValue::ArgValue()
    : type_(ValueType::NONE)
    , str_(nullptr)
    , bytes_(nullptr)
{}

ArgValue::ArgValue(const std::string& s)
    : type_(ValueType::STRING)
    , str_(new std::string(s))
    , bytes_(nullptr)
{}

ArgValue::ArgValue(const char* s)
    : type_(ValueType::STRING)
    , str_(new std::string(s))
    , bytes_(nullptr)
{}

ArgValue::ArgValue(int v)
    : type_(ValueType::INT)
    , i_(v)
    , str_(nullptr)
    , bytes_(nullptr)
{}

ArgValue::ArgValue(double v)
    : type_(ValueType::DOUBLE)
    , d_(v)
    , str_(nullptr)
    , bytes_(nullptr)
{}

ArgValue::ArgValue(bool v)
    : type_(ValueType::BOOL)
    , b_(v)
    , str_(nullptr)
    , bytes_(nullptr)
{}

ArgValue::ArgValue(const std::vector<uint8_t>& bytes)
    : type_(ValueType::BYTES)
    , str_(nullptr)
    , bytes_(new std::vector<uint8_t>(bytes))
{}

ArgValue::ArgValue(const ArgValue& other)
    : type_(ValueType::NONE)
    , str_(nullptr)
    , bytes_(nullptr)
{
    copy_from(other);
}

ArgValue& ArgValue::operator=(const ArgValue& other)
{
    if (this != &other) 
    {
        cleanup();
        copy_from(other);
    }
    return *this;
}

ArgValue::~ArgValue()
{
    cleanup();
}

ArgValue::ValueType ArgValue::type() const
{
    return type_;
}

void ArgValue::cleanup()
{
    if (type_ == ValueType::STRING && str_) 
    {
        delete str_;
        str_ = nullptr;
    }
    else if (type_ == ValueType::BYTES && bytes_)
    {
        delete bytes_;
        bytes_ = nullptr;
    }
    type_ = ValueType::NONE;
}

void ArgValue::copy_from(const ArgValue& other)
{
    type_ = other.type_;

    if (other.type_ == ValueType::STRING) 
    {
        str_   = new std::string(*other.str_);
        bytes_ = nullptr;
    }
    else if (other.type_ == ValueType::INT) 
    {
        i_     = other.i_;
        str_   = nullptr;
        bytes_ = nullptr;
    }
    else if (other.type_ == ValueType::DOUBLE) 
    {
        d_     = other.d_;
        str_   = nullptr;
        bytes_ = nullptr;
    }
    else if (other.type_ == ValueType::BOOL) 
    {
        b_     = other.b_;
        str_   = nullptr;
        bytes_ = nullptr;
    }
    else if (other.type_ == ValueType::BYTES)
    {
        str_   = nullptr;
        bytes_ = new std::vector<uint8_t>(*other.bytes_);
    }
    else 
    {
        str_   = nullptr;
        bytes_ = nullptr;
    }
}

Object::Object()
    : id(next())
{}

std::string Object::next()
{
    static int count = 0;
    return std::to_string(count++);
}

Var::Var()
    : interp_(nullptr)
{}

Var::Var(const Widget& parent, const std::string& type)
    : interp_(parent.interp())
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

StringVar::StringVar(const Widget& parent)
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

BooleanVar::BooleanVar(const Widget& parent)
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

IntVar::IntVar(const Widget& parent)
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

DoubleVar::DoubleVar(const Widget& parent)
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

Widget::Widget()
    : interp_(nullptr)
    , after_id_(0)
{}

Widget::Widget(const Widget& parent, const std::string &type, const std::string& name)
    : interp_(parent.interp_)
    , after_id_(0)
{
    auto parent_name = (parent.full_name() == ".") ? "" : parent.full_name();
    full_name_ = parent_name + "." + (name.empty() ? type : name) + id;
    interp_->invoke({type, full_name_});
}

const std::string& Widget::full_name() const
{
    return full_name_;
}

Widget& Widget::pack(const std::map<std::string, ArgValue> &options)
{
    std::vector<ArgValue> words = {"pack", full_name_};
    for (const auto& kv : options)
    {
        words.push_back("-" + kv.first);
        words.push_back(kv.second);
    }
    interp_->invoke(words);
    return *this;
}

Widget& Widget::pack_forget()
{
    interp_->invoke({"pack", "forget", full_name_});
    return *this;
}

Widget& Widget::grid(const std::map<std::string, ArgValue>& options)
{
    std::vector<ArgValue> words = {"grid", full_name_};
    for (const auto& kv : options)
    {
        words.push_back("-" + kv.first);
        words.push_back(kv.second);
    }
    interp_->invoke(words);
    return *this;
}

Widget& Widget::grid_forget()
{
    interp_->invoke({"grid", "forget", full_name_});
    return *this;
}

Widget& Widget::place(const std::map<std::string, ArgValue> &options)
{
    std::vector<ArgValue> words = {"place", full_name_};
    for (const auto& kv : options)
    {
        words.push_back("-" + kv.first);
        words.push_back(kv.second);
    }
    interp_->invoke(words);
    return *this;
}

Widget& Widget::place_forget()
{
    interp_->invoke({"place", "forget", full_name_});
    return *this;
}

Widget& Widget::config(const std::map<std::string, ArgValue> &options)
{
    if (options.empty())
    {
        return *this;
    }

    std::vector<ArgValue> words = {full_name_, "configure"};
    for (const auto &kv : options)
    {
        words.push_back("-" + kv.first);
        words.push_back(kv.second);
    }
    interp_->invoke(words);
    return *this;
}

Widget& Widget::config(const std::string& name, const ArgValue& value)
{
    config({{name, value}});
    return *this;
}

Widget& Widget::grid_rowconfigure(int row, const std::map<std::string, ArgValue>& options)
{
    std::vector<ArgValue> words = {"grid", "rowconfigure", full_name_, row};
    for (const auto& kv : options)
    {
        words.push_back("-" + kv.first);
        words.push_back(kv.second);
    }
    interp_->invoke(words);
    return *this;
}

Widget& Widget::grid_columnconfigure(int column, const std::map<std::string, ArgValue>& options)
{
    std::vector<ArgValue> words = {"grid", "columnconfigure", full_name_, column};
    for (const auto& kv : options)
    {
        words.push_back("-" + kv.first);
        words.push_back(kv.second);
    }
    interp_->invoke(words);
    return *this;
}

std::string Widget::cget(const std::string& name) const
{
    auto ok  = false;
    auto ret = interp_->invoke({full_name_, "cget", "-" + name}, &ok);
    if (!ok)
        ret = "";
    return ret;
}

Widget& Widget::bind(const std::string& event, std::function<void(const Event&)> callback)
{
    auto cb_name = sanitize(full_name()) + "_" + sanitize(event) + "_bind_cb";
    interp_->register_event_callback(cb_name, callback);
    std::string script = cb_name + " %x %y %X %Y %W %K %k %c %t %D";
    interp_->invoke({"bind", full_name_, event, script});
    return *this;
}

std::string Widget::after(const int& ms, std::function<void()> callback)
{
    auto cb_name = sanitize(full_name()) + "_after_cb_" + std::to_string(after_id_++);
    interp_->register_void_callback(cb_name, callback);
    auto ok  = false;
    auto ret = interp_->invoke({"after", ms, cb_name}, &ok);
    return ret;
}

void Widget::after_idle(std::function<void()> callback)
{
    auto cb_name = sanitize(full_name()) + "_after_idle_cb";
    interp_->register_void_callback(cb_name, callback);
    interp_->invoke({"after", "idle", cb_name});
}

void Widget::after_cancel(const std::string& id)
{
    interp_->invoke({"after", "cancel", id});
}

void Widget::destroy()
{
    interp_->invoke({"destroy", full_name_});
}

int Widget::winfo_width() const
{
    auto ret = interp_->invoke({"winfo", "width", full_name_});
    return safe_stol(ret.c_str());
}

int Widget::winfo_height() const
{
    auto ret = interp_->invoke({"winfo", "height", full_name_});
    return safe_stol(ret.c_str());
}

int Widget::winfo_x() const
{
    auto ret = interp_->invoke({"winfo", "x", full_name_});
    return safe_stol(ret.c_str());
}

int Widget::winfo_y() const
{
    auto ret = interp_->invoke({"winfo", "y", full_name_});
    return safe_stol(ret.c_str());
}

int Widget::winfo_rootx() const
{
    auto ret = interp_->invoke({"winfo", "rootx", full_name_});
    return safe_stol(ret.c_str());
}

int Widget::winfo_rooty() const
{
    auto ret = interp_->invoke({"winfo", "rooty", full_name_});
    return safe_stol(ret.c_str());
}

bool Widget::winfo_exists() const
{
    auto ret = interp_->invoke({"winfo", "exists", full_name_});
    return safe_stol(ret.c_str()) != 0;
}

std::string Widget::winfo_class() const
{
    return interp_->invoke({"winfo", "class", full_name_});
}

std::string Widget::winfo_toplevel() const
{
    return interp_->invoke({"winfo", "toplevel", full_name_});
}

std::vector<std::string> Widget::winfo_children() const
{
    auto result = interp_->invoke({"winfo", "children", full_name_});

    std::vector<std::string> children;
    std::istringstream iss(result);
    std::string child;

    while (iss >> child)
        children.push_back(child);

    return children;
}

Interpreter* Widget::interp() const
{
    return interp_;
}

PhotoImage::PhotoImage()
    : interp_(nullptr)
{}

PhotoImage::PhotoImage(const Widget& parent, const std::map<std::string, ArgValue>& options)
    : interp_(parent.interp())
    , name_("img_" + id)
{
    std::vector<ArgValue> words = {"image", "create", "photo", name_};
    for (const auto& kv : options)
    {
        words.push_back("-" + kv.first);
        words.push_back(kv.second);
    }

    interp_->invoke(words);
}

const std::string& PhotoImage::name() const
{
    return name_;
}

Tk::Tk()
    : Widget()
{
    interp_ = new Interpreter();
    interp_map[std::this_thread::get_id()] = interp_;
    full_name_ = ".";

    title("tk");
    geometry("300x300");
    protocol("WM_DELETE_WINDOW", [this](){quit();});
}

Tk& Tk::title(const std::string& title)
{
    interp_->invoke({"wm", "title", ".", title});
    return *this;
}

Tk& Tk::geometry(const std::string &size)
{
    interp_->invoke({"wm", "geometry", ".", size});
    return *this;
}

std::string Tk::geometry() const
{
    return interp_->invoke({"wm", "geometry", "."});
}

Tk& Tk::protocol(const std::string& name, std::function<void()> handler) 
{
    auto cb_name = "protocol_cb_" + sanitize(name);
    interp_->register_void_callback(cb_name, handler);
    interp_->invoke({"wm", "protocol", ".", name, cb_name});
    return *this;
}

Tk& Tk::resizable(bool width, bool height)
{
    interp_->invoke({"wm", "resizable", ".", (int)(width ? 1 : 0), (int)(height ? 1 : 0)});
    return *this;
}

Tk& Tk::minsize(int width, int height)
{
    interp_->invoke({"wm", "minsize", ".", width, height});
    return *this;
}

Tk& Tk::maxsize(int width, int height)
{
    interp_->invoke({"wm", "maxsize", ".", width, height});
    return *this;
}

Tk& Tk::iconify()
{
    interp_->invoke({"wm", "iconify", "."});
    return *this;
}

Tk& Tk::deiconify()
{
    interp_->invoke({"wm", "deiconify", "."});
    return *this;
}

Tk& Tk::withdraw()
{
    interp_->invoke({"wm", "withdraw", "."});
    return *this;
}

Tk& Tk::state(const std::string& new_state)
{
    interp_->invoke({"wm", "state", ".", new_state});
    return *this;
}

std::string Tk::state() const
{
    return interp_->invoke({"wm", "state", "."});
}

Tk& Tk::attributes(const std::string& name, const std::string& value)
{
    interp_->invoke({"wm", "attributes", ".", name, value});
    return *this;
}

std::string Tk::attributes(const std::string& name) const
{
    return interp_->invoke({"wm", "attributes", ".", name});
}

Tk& Tk::lift()
{
    interp_->invoke({"raise", "."});
    return *this;
}

Tk& Tk::lower()
{
    interp_->invoke({"lower", "."});
    return *this;
}

Tk& Tk::grab_set()
{
    interp_->invoke({"grab", "set", "."});
    return *this;
}

Tk& Tk::grab_release()
{
    interp_->invoke({"grab", "release", "."});
    return *this;
}

Tk& Tk::iconphoto(const std::string& image_name)
{
    interp_->invoke({"wm", "iconphoto", ".", "-default", image_name});
    return *this;
}

Tk& Tk::iconbitmap(const std::string& bitmap_path)
{
    interp_->invoke({"wm", "iconbitmap", ".", bitmap_path});
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

Checkbutton::Checkbutton(const Widget& parent, const std::map<std::string, ArgValue>& options)
    : Widget(parent, "checkbutton", "chk")
{
    config(options);
}

Checkbutton& Checkbutton::text(const std::string& text)
{
    config({{"text", text}});
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

Frame::Frame(const Widget& parent, const std::map<std::string, ArgValue>& options)
    : Widget(parent, "frame", "f")
{
    config(options);
}

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

Frame& Frame::grid_propagate(const bool& value)
{
    interp_->invoke({"grid", "propagate", full_name_, (int)(value ? 1 : 0)});
    return *this;
}

Toplevel::Toplevel(const Widget& parent, const std::map<std::string, ArgValue>& options)
    : Widget(parent, "toplevel")
{
    config(options);
    protocol("WM_DELETE_WINDOW", [this](){destroy();});
}

Toplevel& Toplevel::title(const std::string &title_text)
{
    interp_->invoke({"wm", "title", full_name_, title_text});
    return *this;
}

Toplevel& Toplevel::geometry(const std::string &size)
{
    interp_->invoke({"wm", "geometry", full_name_, size});
    return *this;
}

std::string Toplevel::geometry() const
{
    return interp_->invoke({"wm", "geometry", full_name_});
}

Toplevel& Toplevel::protocol(const std::string& name, std::function<void()> handler) 
{
    std::string callback_name = "protocol_cb_" + sanitize(full_name()) + "_" + sanitize(name);
    interp_->register_void_callback(callback_name, handler);
    interp_->invoke({"wm", "protocol", full_name_, name, callback_name});
    return *this;
}

Toplevel& Toplevel::resizable(bool width, bool height)
{
    interp_->invoke({"wm", "resizable", full_name_, (int)(width ? 1 : 0), (int)(height ? 1 : 0)});
    return *this;
}

Toplevel& Toplevel::minsize(int width, int height)
{
    interp_->invoke({"wm", "minsize", full_name_, width, height});
    return *this;
}

Toplevel& Toplevel::maxsize(int width, int height)
{
    interp_->invoke({"wm", "maxsize", full_name_, width, height});
    return *this;
}

Toplevel& Toplevel::attributes(const std::string& name, const std::string& value)
{
    interp_->invoke({"wm", "attributes", full_name_, name, value});
    return *this;
}

std::string Toplevel::attributes(const std::string& name) const
{
    return interp_->invoke({"wm", "attributes", full_name_, name});
}

Toplevel& Toplevel::iconify()
{
    interp_->invoke({"wm", "iconify", full_name_});
    return *this;
}

Toplevel& Toplevel::deiconify()
{
    interp_->invoke({"wm", "deiconify", full_name_});
    return *this;
}

Toplevel& Toplevel::withdraw()
{
    interp_->invoke({"wm", "withdraw", full_name_});
    return *this;
}

Toplevel& Toplevel::state(const std::string& new_state)
{
    interp_->invoke({"wm", "state", full_name_, new_state});
    return *this;
}

std::string Toplevel::state() const
{
    return interp_->invoke({"wm", "state", full_name_});
}

Toplevel& Toplevel::lift()
{
    interp_->invoke({"raise", full_name_});
    return *this;
}

Toplevel& Toplevel::lower()
{
    interp_->invoke({"lower", full_name_});
    return *this;
}

Toplevel& Toplevel::grab_set()
{
    interp_->invoke({"grab", "set", full_name_});
    return *this;
}

Toplevel& Toplevel::grab_release()
{
    interp_->invoke({"grab", "release", full_name_});
    return *this;
}

Toplevel& Toplevel::iconphoto(const std::string& image_name)
{
    interp_->invoke({"wm", "iconphoto", full_name_, "-default", image_name});
    return *this;
}

Toplevel& Toplevel::iconbitmap(const std::string& bitmap_path)
{
    interp_->invoke({"wm", "iconbitmap", full_name_, bitmap_path});
    return *this;
}

Button::Button(const Widget& parent, const std::map<std::string, ArgValue>& options)
    : Widget(parent, "button", "b")
{
    config(options);
}

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
    config({{"text", text}});        
    return *this;
}

Button& Button::command(std::function<void()> callback)
{
    std::string callback_name =  sanitize(full_name()) + "_void_cb";
    interp_->register_void_callback(callback_name, callback);
    config({{"command", callback_name}});        
    return *this;
}

Canvas::Canvas(const Widget& parent, const std::map<std::string, ArgValue>& options)
    : Widget(parent, "canvas", "c")
{
    config(options);
}

Canvas& Canvas::itemconfig(const std::string& id_or_tag, const std::map<std::string, ArgValue>& options)
{
    std::vector<ArgValue> words = {full_name_, "itemconfigure", id_or_tag};
    for (const auto &kv : options)
    {
        words.push_back("-" + kv.first);
        words.push_back(kv.second);
    }
    interp_->invoke(words);
    return *this;
}

std::string Canvas::create_line(const std::vector<std::array<double, 2>>& points, const std::map<std::string, ArgValue>& options)
{
    std::vector<ArgValue> words = {full_name_, "create", "line"};
    for (const auto& pt : points)
    {
        words.push_back(pt[0]);
        words.push_back(pt[1]);
    }
    for (const auto &kv : options)
    {
        words.push_back("-" + kv.first);
        words.push_back(kv.second);
    }
    return interp_->invoke(words);
}

std::string Canvas::create_line(const int& x1, const int& y1, const int& x2, const int& y2, const std::map<std::string, ArgValue>& options)
{
    std::vector<ArgValue> words = {full_name_, "create", "line", x1, y1, x2, y2};
    for (const auto &kv : options)
    {
        words.push_back("-" + kv.first);
        words.push_back(kv.second);
    }
    return interp_->invoke(words);
}

std::string Canvas::create_oval(const int& x1, const int& y1, const int& x2, const int& y2, const std::map<std::string, ArgValue>& options)
{
    std::vector<ArgValue> words = {full_name_, "create", "oval", x1, y1, x2, y2};
    for (const auto &kv : options)
    {
        words.push_back("-" + kv.first);
        words.push_back(kv.second);
    }
    return interp_->invoke(words);
}

std::string Canvas::create_rectangle(const int& x1, const int& y1, const int& x2, const int& y2, const std::map<std::string, ArgValue>& options)
{
    std::vector<ArgValue> words = {full_name_, "create", "rectangle", x1, y1, x2, y2};
    for (const auto &kv : options)
    {
        words.push_back("-" + kv.first);
        words.push_back(kv.second);
    }
    return interp_->invoke(words);
}

std::string Canvas::create_text(const int& x, const int& y, const std::map<std::string, ArgValue>& options)
{
    std::vector<ArgValue> words = {full_name_, "create", "text", x, y};
    for (const auto &kv : options)
    {
        words.push_back("-" + kv.first);
        words.push_back(kv.second);
    }
    return interp_->invoke(words);
}

std::string Canvas::create_polygon(const std::vector<int>& coords, const std::map<std::string, ArgValue>& options)
{
    std::vector<ArgValue> words = {full_name_, "create", "polygon"};
    for (int c : coords)
        words.push_back(c);
    for (const auto& kv : options)
    {
        words.push_back("-" + kv.first);
        words.push_back(kv.second);
    }
    return interp_->invoke(words);
}

std::string Canvas::create_arc(int x1, int y1, int x2, int y2, const std::map<std::string, ArgValue>& options)
{
    std::vector<ArgValue> words = {full_name_, "create", "arc", x1, y1, x2, y2};
    for (const auto& kv : options)
    {
        words.push_back("-" + kv.first);
        words.push_back(kv.second);
    }
    return interp_->invoke(words);
}

std::string Canvas::create_image(int x, int y, const std::map<std::string, ArgValue>& options)
{
    std::vector<ArgValue> words = {full_name_, "create", "image", x, y};
    for (const auto& kv : options)
    {
        words.push_back("-" + kv.first);
        words.push_back(kv.second);
    }
    return interp_->invoke(words);
}

std::string Canvas::create_window(int x, int y, const Widget& widget, const std::map<std::string, ArgValue>& options)
{
    std::vector<ArgValue> words = {full_name_, "create", "window", x, y, "-window", widget.full_name()};
    for (const auto& kv : options)
    {
        words.push_back("-" + kv.first);
        words.push_back(kv.second);
    }
    return interp_->invoke(words);
}

std::vector<std::string> Canvas::find_overlapping(int x1, int y1, int x2, int y2) const
{
    auto result = interp_->invoke({full_name_, "find", "overlapping", x1, y1, x2, y2});
    std::vector<std::string> ids;
    std::istringstream iss(result);
    std::string id;
    while (iss >> id)
        ids.push_back(id);
    return ids;
}

std::vector<std::string> Canvas::find_closest(int x, int y) const
{
    auto result = interp_->invoke({full_name_, "find", "closest", x, y});
    std::vector<std::string> ids;
    std::istringstream iss(result);
    std::string id;
    while (iss >> id)
        ids.push_back(id);
    return ids;
}

Canvas& Canvas::addtag(const std::string& tag, const std::string& where, const std::string& target)
{
    interp_->invoke({full_name_, "addtag", tag, where, target});
    return *this;
}

Canvas& Canvas::dtag(const std::string& tag, const std::string& target)
{
    interp_->invoke({full_name_, "dtag", target, tag});
    return *this;
}

std::vector<std::string> Canvas::gettags(const std::string& id) const
{
    auto result = interp_->invoke({full_name_, "gettags", id});
    std::vector<std::string> tags;
    std::istringstream iss(result);
    std::string tag;
    while (iss >> tag)
        tags.push_back(tag);
    return tags;
}

Canvas& Canvas::coords(const std::string& item_id, const std::vector<int>& coords)
{
    std::vector<ArgValue> words = {full_name_, "coords", item_id};
    for (int c : coords)
        words.push_back(c);
    interp_->invoke(words);
    return *this;
}

Canvas& Canvas::move(const std::string& id_or_tag, const int& x, const int& y)
{
    interp_->invoke({full_name_, "move", id_or_tag, x, y});
    return *this;
}

Canvas& Canvas::moveto(const std::string& id_or_tag, const int& x, const int& y)
{
    interp_->invoke({full_name_, "moveto", id_or_tag, x, y});
    return *this;
}

Canvas& Canvas::scale(const std::string& id_or_tag, const int& x, const int& y, const double& xscale, const double& yscale)
{
    interp_->invoke({full_name_, "scale", id_or_tag, x, y, xscale, yscale});
    return *this;
}

Canvas& Canvas::rotate(const std::string& id_or_tag, const int& x, const int& y, const double& angle)
{
    interp_->invoke({full_name_, "rotate", id_or_tag, x, y, angle});
    return *this;
}

Canvas& Canvas::erase(const std::string& id_or_tag)
{
    interp_->invoke({full_name_, "delete", id_or_tag});
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

Entry::Entry()
    : text_var_(nullptr)
{}

Entry::Entry(const Widget& parent, const std::map<std::string, ArgValue>& options)
    : Widget(parent, "entry", "e")
    , text_var_(nullptr)
{
    config(options);
}

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
    interp_->invoke({full_name_, "icursor", index});
    return *this;
}

Entry& Entry::insert(const std::string& index, const std::string& text) 
{
    interp_->invoke({full_name_, "insert", index, text});
    return *this;
}

int Entry::index(const std::string& index) const 
{
    auto ok  = false;
    auto ret = interp_->invoke({full_name_, "index", index}, &ok);
    if (!ok)
        return -1;
    return std::stol(ret);
}

Entry& Entry::erase(const std::string& start, const std::string& end) 
{
    std::vector<ArgValue> words = {full_name_, "delete", start};
    if (!end.empty())
        words.push_back(end);
    interp_->invoke(words);
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
        return text_var_->get_var();
    auto ok  = false;
    auto ret = interp_->invoke({full_name_, "get"}, &ok);
    return ret;
}

Label::Label(const Widget& parent, const std::map<std::string, ArgValue>& options)
    : Widget(parent, "label", "l") 
{
    config(options);
}

Label& Label::text(const std::string &text)
{
    config({{"text", text}});
    return *this;
}

Listbox::Listbox(const Widget& parent, const std::map<std::string, ArgValue>& options)
    : Widget(parent, "listbox", "listbox") 
{
    config(options);
}

Listbox& Listbox::insert(int index, const std::string& item) 
{
    interp_->invoke({full_name_, "insert", index, item});
    return *this;
}

Listbox& Listbox::erase(int start, int end) 
{
    interp_->invoke({full_name_, "delete", start, end});
    return *this;
}

std::vector<int> Listbox::curselection() const 
{
    std::string result = interp_->invoke({full_name_, "curselection"});
    return {};
}

std::string Listbox::get(int index) const 
{
    return interp_->invoke({full_name_, "get", index});
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

Menu::Menu(const Widget& parent, const std::map<std::string, ArgValue>& options)
    : Widget(parent, "menu", "menu")
{
    config(options);
}

Menu& Menu::add_command(const std::map<std::string, ArgValue>& options)
{
    std::vector<ArgValue> words = {full_name_, "add", "command"};
    for (const auto& kv : options)
    {
        words.push_back("-" + kv.first);
        words.push_back(kv.second);
    }
    interp_->invoke(words);
    return *this;
}

Menu& Menu::add_cascade(const std::map<std::string, ArgValue>& options)
{
    std::vector<ArgValue> words = {full_name_, "add", "cascade"};
    for (const auto& kv : options)
    {
        words.push_back("-" + kv.first);
        words.push_back(kv.second);
    }
    interp_->invoke(words);
    return *this;
}

Menu& Menu::add_separator()
{
    interp_->invoke({full_name_, "add", "separator"});
    return *this;
}

Menu& Menu::delete_item(const std::string& index)
{
    interp_->invoke({full_name_, "delete", index});
    return *this;
}

Menubutton::Menubutton(const Widget& parent)
    : Widget(parent, "menubutton", "mb")
{}

Menubutton& Menubutton::menu(Menu* menu)
{
    config({{"menu", menu->full_name()}});
    return *this;
}

Message::Message(const Widget& parent)
    : Widget(parent, "message", "msg")
{}

Message& Message::text(const std::string& text)
{
    config({{"text", text}});
    return *this;
}

PanedWindow::PanedWindow(const Widget& parent)
    : Widget(parent, "panedwindow", "pw")
{}

PanedWindow& PanedWindow::orient(const std::string& dir)
{
    config({{"orient", dir}});
    return *this;
}

PanedWindow& PanedWindow::add(const Widget& child, const std::map<std::string, ArgValue>& options)
{
    std::vector<ArgValue> words = {full_name_, "add", child.full_name()};
    for (const auto& kv : options)
    {
        words.push_back("-" + kv.first);
        words.push_back(kv.second);
    }
    interp_->invoke(words);
    return *this;
}

PanedWindow& PanedWindow::forget(const Widget& child)
{
    interp_->invoke({full_name_, "forget", child.full_name()});
    return *this;
}

Radiobutton::Radiobutton(const Widget& parent, const std::map<std::string, ArgValue>& options)
    : Widget(parent, "radiobutton", "rb")
{
    config(options);
}

Radiobutton& Radiobutton::text(const std::string& text)
{
    config({{"text", text}});
    return *this;
}

Radiobutton& Radiobutton::variable(Var* var)
{
    config({{"variable", var->name()}});
    return *this;
}

Radiobutton& Radiobutton::value(const std::string& val)
{
    interp_->invoke({full_name_, "configure", "-value", val});
    return *this;
}

Radiobutton& Radiobutton::command(std::function<void()> callback)
{
    auto cb = sanitize(full_name()) + "_rb_cb";
    interp_->register_void_callback(cb, callback);
    config({{"command", cb}});
    return *this;
}

Scale::Scale(const Widget& parent, const std::map<std::string, ArgValue>& options)
    : Widget(parent, "scale", "scale") 
{
    config(options);
}
    
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

Scrollbar::Scrollbar(const Widget& parent, const std::map<std::string, ArgValue>& options) 
    : Widget(parent, "scrollbar", "scrollbar") 
{
    config(options);
}

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
    interp_->invoke({full_name_, "set", args});
    return *this;
}

Spinbox::Spinbox(const Widget& parent, const std::map<std::string, ArgValue>& options)
    : Widget(parent, "spinbox", "spinbox")
{
    config(options);
}

Spinbox& Spinbox::from(double val)
{
    config({{"from", val}});
    return *this;
}

Spinbox& Spinbox::to(double val)
{
    config({{"to", val}});
    return *this;
}

Spinbox& Spinbox::increment(double val)
{
    config({{"increment", val}});
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

Text::Text(const Widget& parent, const std::map<std::string, ArgValue>& options) 
    : Widget(parent, "text", "text") 
{
    config(options);
}

Text& Text::insert(const std::string& index, const std::string& text) 
{
    interp_->invoke({full_name_, "insert", index, text});
    return *this;
}

std::string Text::get(const std::string& start, const std::string& end) const 
{
    return interp_->invoke({full_name_, "get", start, end});
}

Text& Text::erase(const std::string& start, const std::string& end) 
{
    interp_->invoke({full_name_, "delete", start, end});
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
    interp_->invoke({full_name_, "yview", args});
    return *this;
}

Text& Text::wrap(const std::string& mode) 
{
    config({{"wrap", mode}});
    return *this;
}

Text& Text::tag_add(const std::string& tag, const std::string& start, const std::string& end)
{
    interp_->invoke({full_name_, "tag", "add", tag, start, end});
    return *this;
}

Text& Text::tag_remove(const std::string& tag, const std::string& start, const std::string& end)
{
    interp_->invoke({full_name_, "tag", "remove", tag, start, end});
    return *this;
}

Text& Text::tag_config(const std::string& tag, const std::map<std::string, ArgValue>& options)
{
    std::vector<ArgValue> words = {full_name_, "tag", "configure", tag};
    for (const auto& kv : options)
    {
        words.push_back("-" + kv.first);
        words.push_back(kv.second);
    }
    interp_->invoke(words);
    return *this;
}

Text& Text::mark_set(const std::string& mark, const std::string& index)
{
    interp_->invoke({full_name_, "mark", "set", mark, index});
    return *this;
}

Text& Text::mark_unset(const std::string& mark)
{
    interp_->invoke({full_name_, "mark", "unset", mark});
    return *this;
}

std::string Text::search(const std::string& pattern, const std::string& index, const std::map<std::string, ArgValue>& options)
{
    std::vector<ArgValue> words = {full_name_, "search"};
    for (const auto& kv : options)
    {
        words.push_back("-" + kv.first);
        words.push_back(kv.second);
    }
    words.push_back(pattern);
    words.push_back(index);
    auto ok  = false;
    auto ret = interp_->invoke(words, &ok);
    return ok ? ret : "";
}

namespace ttk
{

Font::Font()
    : interp_(nullptr)
{}

Font::Font(const Widget& parent, const std::map<std::string, ArgValue>& option)
    : name_("font_" + id)
    , interp_(parent.interp())
{
    interp_->invoke({"font", "create", name_});
    if (!option.empty())
    {
        config(option);
    }
}

Font& Font::config(const std::map<std::string, ArgValue>& option)
{
    std::vector<ArgValue> words = {"font", "configure", name_};
    for (const auto &kv : option)
    {
        words.push_back("-" + kv.first);
        words.push_back(kv.second);
    }
    interp_->invoke(words);
    return *this;
}

Font& Font::size(const int& size)
{
    return config({{"size", size}});
}

Font& Font::weight(const std::string& weight)
{
    return config({{"weight", weight}});
}

Font& Font::family(const std::string& family)
{
    return config({{"family", family}});
}

Font& Font::slant(const std::string& slant)
{
    return config({{"slant", slant}});
}

Font& Font::underline(const int& underline)
{
    return config({{"underline", underline}});
}

Font& Font::overstrike(const int& overstrike)
{
    return config({{"overstrike", overstrike}});
}

const std::string& Font::name() const
{
    return name_;
}

Button::Button(const Widget& parent, const std::map<std::string, ArgValue>& options)
    : Widget(parent, "ttk::button", "ttk_button")
{
    config(options);
}

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
    config({{"text", text}});        
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

Checkbutton::Checkbutton(const Widget& parent, const std::map<std::string, ArgValue>& options)
    : Widget(parent, "ttk::checkbutton", "ttk_checkbutton")
{
    config(options);
}

Checkbutton& Checkbutton::text(const std::string& text)
{
    config({{"text", text}});
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

Combobox::Combobox()
    : text_var_(nullptr)
{}

Combobox::Combobox(const Widget& parent, const std::map<std::string, ArgValue>& options)
    : Widget(parent, "ttk::combobox", "ttk_combobox")
    , text_var_(nullptr)
{
    config(options);
}

Combobox& Combobox::values(const std::vector<std::string>& items) 
{
    std::vector<ArgValue> words = {full_name_, "configure", "-values"};
    // Tcl リスト形式で一括渡し: 各要素を個別 ArgValue として渡す
    // Tcl_EvalObjv では -values {a b c} と等価な渡し方をする必要があるため
    // リストオブジェクトを構築する
    std::string list_str;
    for (const auto& item : items)
    {
        if (!list_str.empty()) list_str += " ";
        list_str += "{" + item + "}";
    }
    words.push_back(list_str);
    interp_->invoke(words);
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

Combobox& Combobox::set(const std::string& value) 
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

Combobox& Combobox::insert(const std::string& index, const std::string& text) 
{
    interp_->invoke({full_name_, "insert", index, text});
    return *this;
}

std::string Combobox::get() const 
{
    return interp_->invoke({full_name_, "get"});
}

Combobox& Combobox::erase(const std::string& start, const std::string& end) 
{
    interp_->invoke({full_name_, "delete", start, end});
    return *this;
}

int Combobox::current() const
{
    const auto val = interp_->invoke({full_name_, "current"});
    return safe_stol(val.c_str());
}

Combobox& Combobox::current(const int& idx)
{
    interp_->invoke({full_name_, "current", idx});
    return *this;    
}

Entry::Entry()
    : text_var_(nullptr)
{}

Entry::Entry(const Widget& parent, const std::map<std::string, ArgValue>& options)
    : Widget(parent, "ttk::entry", "ttk_entry")
    , text_var_(nullptr)
{
    config(options);
}

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
    interp_->invoke({full_name_, "icursor", index});
    return *this;
}

Entry& Entry::insert(const std::string& index, const std::string& text) 
{
    interp_->invoke({full_name_, "insert", index, text});
    return *this;
}

int Entry::index(const std::string& index) const 
{
    auto ok  = false;
    auto ret = interp_->invoke({full_name_, "index", index}, &ok);
    if (!ok)
        return -1;
    return std::stol(ret);
}

Entry& Entry::erase(const std::string& start, const std::string& end) 
{
    std::vector<ArgValue> words = {full_name_, "delete", start};
    if (!end.empty())
        words.push_back(end);
    interp_->invoke(words);
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

    auto ok  = false;
    auto ret = interp_->invoke({full_name_, "get"}, &ok);
    return ret;
}

Frame::Frame(const Widget& parent, const std::map<std::string, ArgValue>& options)
    : Widget(parent, "ttk::frame", "ttk_frame")
{
    config(options);
}

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


Label::Label(const Widget& parent, const std::map<std::string, ArgValue>& options)
    : Widget(parent, "ttk::label", "tl") 
{
    config(options);
}

Label& Label::text(const std::string &text)
{
    config({{"text", text}});
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

Labelframe::Labelframe(const Widget& parent, const std::map<std::string, ArgValue>& options)
    : Widget(parent, "ttk::labelframe", "ttk_labelframe")
{
    config(options);
}

Labelframe& Labelframe::text(const std::string& text)
{
    config({{"text", text}});
    return *this;
}

Notebook::Notebook(const Widget& parent, const std::map<std::string, ArgValue>& options) 
    : Widget(parent, "ttk::notebook", "ttk_notebook") 
{
    config(options);
}

Notebook& Notebook::add_tab(const Widget& child, const std::string& label) 
{
    interp_->invoke({full_name_, "add", child.full_name(), "-text", label});
    return *this;
}

Notebook& Notebook::select(int index) 
{
    interp_->invoke({full_name_, "select", index});
    return *this;
}

Progressbar::Progressbar(const Widget& parent, const std::map<std::string, ArgValue>& options)
    : Widget(parent, "ttk::progressbar", "ttk_progress")
{
    config(options);
}

Progressbar& Progressbar::mode(const std::string& mode)
{
    config({{"mode", mode}});   // "determinate" or "indeterminate"
    return *this;
}

Progressbar& Progressbar::value(double v)
{
    config({{"value", v}});
    return *this;
}

Progressbar& Progressbar::start(int interval)
{
    interp_->invoke({full_name_, "start", interval});
    return *this;
}

Progressbar& Progressbar::stop()
{
    interp_->invoke({full_name_, "stop"});
    return *this;
}

Progressbar& Progressbar::step(double amount)
{
    interp_->invoke({full_name_, "step", amount});
    return *this;
}

Radiobutton::Radiobutton(const Widget& parent, const std::map<std::string, ArgValue>& options)
    : Widget(parent, "ttk::radiobutton", "ttk_radiobutton")
{
    config(options);
}

Radiobutton& Radiobutton::text(const std::string& text)
{
    config({{"text", text}});
    return *this;
}

Radiobutton& Radiobutton::variable(Var* var)
{
    config({{"variable", var->name()}});
    return *this;
}

Radiobutton& Radiobutton::value(const std::string& val)
{
    interp_->invoke({full_name_, "configure", "-value", val});
    return *this;
}

Radiobutton& Radiobutton::command(std::function<void()> callback)
{
    auto cb = sanitize(full_name()) + "_rb_cb";
    interp_->register_void_callback(cb, callback);
    config({{"command", cb}});
    return *this;
}

Separator::Separator(const Widget& parent, const std::map<std::string, ArgValue>& options)
    : Widget(parent, "ttk::separator", "ttk_separator") 
{}

Scale::Scale(const Widget& parent, const std::map<std::string, ArgValue>& options)
    : Widget(parent, "ttk::scale", "ttk_scale") 
{
    config(options);
}
    
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

Scrollbar::Scrollbar(const Widget& parent, const std::map<std::string, ArgValue>& options) 
    : Widget(parent, "ttk::scrollbar", "ttk_scrollbar") 
{
    config(options);
}

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
    interp_->invoke({full_name_, "set", args});
    return *this;
}

Spinbox::Spinbox(const Widget& parent, const std::map<std::string, ArgValue>& options)
    : Widget(parent, "ttk::spinbox", "ttk_spinbox")
{
    config(options);
}

Spinbox& Spinbox::from(double val)
{
    config({{"from", val}});
    return *this;
}

Spinbox& Spinbox::to(double val)
{
    config({{"to", val}});
    return *this;
}

Spinbox& Spinbox::increment(double val)
{
    config({{"increment", val}});
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

Sizegrip::Sizegrip(const Widget& parent, const std::map<std::string, ArgValue>& options)
    : Widget(parent, "ttk::sizegrip", "ttk_sizegrip")
{
    config(options);
}

Treeview::Treeview(const Widget& parent, const std::map<std::string, ArgValue>& options)
    : Widget(parent, "ttk::treeview", "tv")
{
    config(options);
}

Treeview& Treeview::insert(const std::string& parent, const std::string& index, const std::string& iid, const std::map<std::string, ArgValue>& options)
{
    std::vector<ArgValue> words = {full_name_, "insert", parent, index, "-id", iid};
    for (const auto& kv : options)
    {
        words.push_back("-" + kv.first);
        words.push_back(kv.second);
    }
    interp_->invoke(words);
    return *this;
}

Treeview& Treeview::erase(const std::string& iid)
{
    interp_->invoke({full_name_, "delete", iid});
    return *this;
}

Treeview& Treeview::item(const std::string& iid, const std::map<std::string, ArgValue>& options)
{
    if (options.empty())
    {
        // getter 的な使い方をしたい場合は、必要に応じて別メソッドを追加してもよい
        interp_->invoke({full_name_, "item", iid});
        return *this;
    }

    std::vector<ArgValue> words = {full_name_, "item", iid};
    for (const auto& kv : options)
    {
        words.push_back("-" + kv.first);
        words.push_back(kv.second);
    }
    interp_->invoke(words);
    return *this;
}

Treeview& Treeview::heading(const std::string& column, const std::map<std::string, ArgValue>& options)
{
    std::vector<ArgValue> words = {full_name_, "heading", column};
    for (const auto& kv : options)
    {
        words.push_back("-" + kv.first);
        words.push_back(kv.second);
    }
    interp_->invoke(words);
    return *this;
}

Treeview& Treeview::column(const std::string& column, const std::map<std::string, ArgValue>& options)
{
    std::vector<ArgValue> words = {full_name_, "column", column};
    for (const auto& kv : options)
    {
        words.push_back("-" + kv.first);
        words.push_back(kv.second);
    }
    interp_->invoke(words);
    return *this;
}

std::vector<std::string> Treeview::selection() const
{
    auto ret = interp_->invoke({full_name_, "selection"});

    std::vector<std::string> out;
    std::istringstream iss(ret);
    std::string token;

    while (iss >> token)
        out.push_back(token);

    return out;
}

Treeview& Treeview::set(const std::string& iid, const std::string& column, const ArgValue& value)
{
    interp_->invoke({full_name_, "set", iid, column, value});
    return *this;
}

std::string Treeview::set(const std::string& iid, const std::string& column) const
{
    return interp_->invoke({full_name_, "set", iid, column});
}

Treeview& Treeview::move(const std::string& iid, const std::string& parent, const std::string& index)
{
    interp_->invoke({full_name_, "move", iid, parent, index});
    return *this;
}

Treeview& Treeview::detach(const std::string& iid)
{
    interp_->invoke({full_name_, "detach", iid});
    return *this;
}

Treeview& Treeview::reattach(const std::string& iid, const std::string& parent, const std::string& index)
{
    interp_->invoke({full_name_, "reattach", iid, parent, index});
    return *this;
}

std::vector<std::string> Treeview::get_children(const std::string& iid) const
{
    auto ret = interp_->invoke({full_name_, "children", iid});
    std::vector<std::string> out;
    std::istringstream iss(ret);
    std::string token;

    while (iss >> token)
        out.push_back(token);

    return out;
}

std::string Treeview::parent(const std::string& iid) const
{
    return interp_->invoke({full_name_, "parent", iid});
}

int Treeview::index(const std::string& iid) const
{
    auto ret = interp_->invoke({full_name_, "index", iid});
    return std::stoi(ret);
}

Treeview& Treeview::focus(const std::string& iid)
{
    interp_->invoke({full_name_, "focus", iid});
    return *this;
}

std::string Treeview::focus() const
{
    return interp_->invoke({full_name_, "focus"});
}

Treeview& Treeview::tag_configure(const std::string& tag, const std::map<std::string, ArgValue>& options)
{
    std::vector<ArgValue> words = {full_name_, "tag", "configure", tag};
    for (const auto& kv : options)
    {
        words.push_back("-" + kv.first);
        words.push_back(kv.second);
    }
    interp_->invoke(words);
    return *this;
}

Treeview& Treeview::tag_bind(const std::string& tag, const std::string& event, std::function<void(const Event&)> callback)
{
    auto cb_name = sanitize(full_name()) + "_tag_" + sanitize(tag) + "_" + sanitize(event);

    interp_->register_event_callback(cb_name, callback);

    std::string script = cb_name + " %x %y %X %Y %W %K %k %c %t %D";

    interp_->invoke({full_name_, "tag", "bind", tag, event, script});
    return *this;
}

std::string Treeview::identify_row(int y) const
{
    return interp_->invoke({full_name_, "identify", "row", y});
}

std::string Treeview::identify_column(int x) const
{
    return interp_->invoke({full_name_, "identify", "column", x});
}

std::vector<int> Treeview::bbox(const std::string& iid, const std::string& column) const
{
    auto ret = interp_->invoke({full_name_, "bbox", iid, column});
    std::vector<int> out;
    std::istringstream iss(ret);
    int v;

    while (iss >> v)
        out.push_back(v);

    return out;
}

} // ttk

namespace colorchooser
{

std::string askcolor(const std::map<std::string, ArgValue>& options)
{
    auto* interp = cpp_tk::interp_map[std::this_thread::get_id()];
    if (!interp)
        return "";

    std::vector<ArgValue> words = {"tk_chooseColor"};
    for (const auto& kv : options)
    {
        words.push_back("-" + kv.first);
        words.push_back(kv.second);
    }

    bool ok = false;
    auto ret = interp->invoke(words, &ok);
    return ok ? ret : "";
}

} // namespace colorchooser

namespace filedialog
{

static std::string invoke_dialog(const std::string& cmd_name, const std::map<std::string, ArgValue>& options)
{
    auto* interp = interp_map[std::this_thread::get_id()];
    if (!interp) return "";
    std::vector<ArgValue> words = {cmd_name};
    for (const auto& kv : options)
    {
        words.push_back("-" + kv.first);
        words.push_back(kv.second);
    }
    return interp->invoke(words);
}

std::string askopenfile(const std::map<std::string, ArgValue>& options) 
{
    return invoke_dialog("tk_getOpenFile", options);
}

std::string asksaveasfilename(const std::map<std::string, ArgValue>& options) 
{
    return invoke_dialog("tk_getSaveFile", options);
}

std::string askdirectory(const std::map<std::string, ArgValue>& options) 
{
    return invoke_dialog("tk_chooseDirectory", options);
}

} // filedialog

namespace messagebox 
{

static std::string msgbox(const std::string& type, const std::string& icon, const std::string& title, const std::string& message)
{
    auto* interp = interp_map[std::this_thread::get_id()];
    if (!interp) return "";
    return interp->invoke({"tk_messageBox", "-type", type, "-icon", icon, "-title", title, "-message", message});
}

std::string showinfo(const std::string& title, const std::string& message) 
{
    return msgbox("ok", "info", title, message);
}

std::string showwarning(const std::string& title, const std::string& message) 
{
    return msgbox("ok", "warning", title, message);
}

std::string showerror(const std::string& title, const std::string& message) 
{
    return msgbox("ok", "error", title, message);
}

std::string askquestion(const std::string& title, const std::string& message) 
{
    return msgbox("yesno", "question", title, message);
}

bool askyesno(const std::string& title, const std::string& message) 
{
    return askquestion(title, message) == "yes";
}

bool askokcancel(const std::string& title, const std::string& message) 
{
    return msgbox("okcancel", "question", title, message) == "ok";
}

bool askretrycancel(const std::string& title, const std::string& message) 
{
    return msgbox("retrycancel", "warning", title, message) == "retry";
}

} // messagebox

} // cpp_tk