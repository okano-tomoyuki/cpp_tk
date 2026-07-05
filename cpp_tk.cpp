#include <cstring>
#include <string>
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
        case cpp_tk::ArgValue::ValueType::STRING_LIST:
        {
            const auto& list = v.as_string_list();
            Tcl_Obj* list_obj = Tcl_NewListObj(0, nullptr);
            for (const auto& item : list)
            {
                Tcl_ListObjAppendElement(interp, list_obj, Tcl_NewStringObj(item.c_str(), (int)item.size()));
            }
            return list_obj;
        }
        default:
            return Tcl_NewObj();
    }
}

}

namespace cpp_tk
{

std::unordered_map<std::thread::id, Interpreter*> interp_map;

// Var/PhotoImage/font::Font/ttk::Styleは、Widgetとは別に自分専用のInterpreter*を持つ。
// 現在のスレッドに紐づくInterpreterは常にひとつだけなので、Widget(親)の内部を覗く
// (friend等)必要はなく、interp_mapをこのスレッドのIDで直接引けば同じ答えが得られる。
Interpreter* current_interp()
{
    auto it = interp_map.find(std::this_thread::get_id());
    return it != interp_map.end() ? it->second : nullptr;
}

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

    std::string call(const std::vector<ArgValue>& words, bool* success = nullptr)
    {
        std::vector<Tcl_Obj*> objv;
        objv.reserve(words.size());
        for (const auto& w : words)
            objv.push_back(make_obj(interp_, w));

        for (auto* obj : objv)
            Tcl_IncrRefCount(obj);

        int code = Tcl_EvalObjv(interp_, (int)objv.size(), objv.data(), 0);

        bool ok = (code == TCL_OK);
        if (!ok)
        {
            std::cerr << "Tcl Error: " << Tcl_GetStringResult(interp_) << " (command:";
            for (auto* obj : objv)
                std::cerr << " " << Tcl_GetString(obj);
            std::cerr << ")" << std::endl;
        }

        for (auto* obj : objv)
            Tcl_DecrRefCount(obj);

        if (success)
            *success = ok;
        return Tcl_GetStringResult(interp_);
    }

    // Tclのリスト形式の文字列(要素にスペースを含む場合は{}や""で囲まれる)を、
    // 単純な空白split(既存のwinfo_children等が使っている方式)では壊れてしまうため、
    // Tcl_SplitListで正しく要素分解する。
    std::vector<std::string> split_list(const std::string& list) const
    {
        int argc = 0;
        const char** argv = nullptr;
        std::vector<std::string> result;
        if (Tcl_SplitList(interp_, list.c_str(), &argc, &argv) == TCL_OK)
        {
            result.reserve(argc);
            for (int i = 0; i < argc; ++i)
                result.emplace_back(argv[i]);
            Tcl_Free(reinterpret_cast<char*>(argv));
        }
        return result;
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
                std::string args;
                for (int i = 1; i < argc; ++i)
                {
                    if (i > 1) args += ' ';
                    args += argv[i];
                }
                it->second(args);
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

    // Entry::validate()等、Tcl側にbool(0/1)を返す必要があるコールバック(validatecommand等)用。
    // 他のregister_*_callbackと異なり、Tclコマンドの戻り値そのものをcallbackの結果にする。
    void register_bool_callback(const std::string& name, std::function<bool(const std::string&)> callback)
    {
        bool_callback_map_[name] = callback;
        Tcl_CreateCommand(interp_, name.c_str(), [](ClientData client_data, Tcl_Interp* interp, int argc, const char* argv[]) -> int {
            auto* self = static_cast<Interpreter*>(client_data);
            auto it = self->bool_callback_map_.find(argv[0]);
            bool result = true;
            if (it != self->bool_callback_map_.end())
            {
                std::string arg = (argc > 1) ? argv[1] : "";
                result = it->second(arg);
            }
            Tcl_SetObjResult(interp, Tcl_NewBooleanObj(result ? 1 : 0));
            return TCL_OK;
        }, this, nullptr);
    }

private:
    Tcl_Interp* interp_;

    std::unordered_map<std::string, std::function<void(const Event&)>>          event_callback_map_;

    std::unordered_map<std::string, std::function<void()>>                      void_callback_map_;

    std::unordered_map<std::string, std::function<void(const int&)>>            int_callback_map_;

    std::unordered_map<std::string, std::function<void(const double&)>>         double_callback_map_;

    std::unordered_map<std::string, std::function<bool(const std::string&)>>    bool_callback_map_;

    std::unordered_map<std::string, std::function<void(const std::string&)>>    string_callback_map_;
};

ArgValue::ArgValue()
    : type_(ValueType::NONE)
    , str_(nullptr)
    , bytes_(nullptr)
    , list_(nullptr)
{}

ArgValue::ArgValue(const std::string& s)
    : type_(ValueType::STRING)
    , str_(new std::string(s))
    , bytes_(nullptr)
    , list_(nullptr)
{}

ArgValue::ArgValue(const char* s)
    : type_(ValueType::STRING)
    , str_(new std::string(s))
    , bytes_(nullptr)
    , list_(nullptr)
{}

ArgValue::ArgValue(int v)
    : type_(ValueType::INT)
    , i_(v)
    , str_(nullptr)
    , bytes_(nullptr)
    , list_(nullptr)
{}

ArgValue::ArgValue(double v)
    : type_(ValueType::DOUBLE)
    , d_(v)
    , str_(nullptr)
    , bytes_(nullptr)
    , list_(nullptr)
{}

ArgValue::ArgValue(bool v)
    : type_(ValueType::BOOL)
    , b_(v)
    , str_(nullptr)
    , bytes_(nullptr)
    , list_(nullptr)
{}

ArgValue::ArgValue(const std::vector<uint8_t>& bytes)
    : type_(ValueType::BYTES)
    , str_(nullptr)
    , bytes_(new std::vector<uint8_t>(bytes))
    , list_(nullptr)
{}

ArgValue::ArgValue(const std::vector<std::string>& list)
    : type_(ValueType::STRING_LIST)
    , str_(nullptr)
    , bytes_(nullptr)
    , list_(new std::vector<std::string>(list))
{}

ArgValue::ArgValue(const ArgValue& other)
    : type_(ValueType::NONE)
    , str_(nullptr)
    , bytes_(nullptr)
    , list_(nullptr)
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
    else if (type_ == ValueType::STRING_LIST && list_)
    {
        delete list_;
        list_ = nullptr;
    }
    type_ = ValueType::NONE;
}

void ArgValue::copy_from(const ArgValue& other)
{
    type_ = other.type_;
    str_  = nullptr;
    bytes_ = nullptr;
    list_  = nullptr;

    if (other.type_ == ValueType::STRING)
    {
        str_ = new std::string(*other.str_);
    }
    else if (other.type_ == ValueType::INT)
    {
        i_ = other.i_;
    }
    else if (other.type_ == ValueType::DOUBLE)
    {
        d_ = other.d_;
    }
    else if (other.type_ == ValueType::BOOL)
    {
        b_ = other.b_;
    }
    else if (other.type_ == ValueType::BYTES)
    {
        bytes_ = new std::vector<uint8_t>(*other.bytes_);
    }
    else if (other.type_ == ValueType::STRING_LIST)
    {
        list_ = new std::vector<std::string>(*other.list_);
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
    : interp_(current_interp())
    , name_(type + "_var_" + id)
{}

const std::string& Var::name() const
{
    return name_;
}

std::string Var::get_var() const
{
    if (interp_ == nullptr)
    {
        std::cerr << "cpp_tk Error: get_var() called on an uninitialized Var (interp_ == nullptr)." << std::endl;
        return {};
    }
    return interp_->get_var(name_);
}

void Var::set_var(const std::string& value)
{
    if (interp_ == nullptr)
    {
        std::cerr << "cpp_tk Error: set_var() called on an uninitialized Var (interp_ == nullptr)." << std::endl;
        return;
    }
    interp_->set_var(name_, value);
}

void Var::trace_var(std::function<void(const std::string&)> callback)
{
    if (interp_ == nullptr)
    {
        std::cerr << "cpp_tk Error: trace() called on an uninitialized Var (interp_ == nullptr)." << std::endl;
        return;
    }
    interp_->trace_var(name_, callback);
}

void Var::trace_var(std::function<void(const int&)> callback)
{
    if (interp_ == nullptr)
    {
        std::cerr << "cpp_tk Error: trace() called on an uninitialized Var (interp_ == nullptr)." << std::endl;
        return;
    }
    interp_->trace_var(name_, callback);
}

void Var::trace_var(std::function<void(const double&)> callback)
{
    if (interp_ == nullptr)
    {
        std::cerr << "cpp_tk Error: trace() called on an uninitialized Var (interp_ == nullptr)." << std::endl;
        return;
    }
    interp_->trace_var(name_, callback);
}

StringVar::StringVar(const Widget& parent)
    : Var(parent, "string")
{
    set_var("");
}

void StringVar::set(const std::string &value)
{
    set_var(value);
}

std::string StringVar::get() const
{
    return get_var();
}

void StringVar::trace(std::function<void(const std::string&)> callback)
{
    trace_var(callback);
}

BooleanVar::BooleanVar(const Widget& parent)
    : Var(parent, "bool")
{
    set_var("0");
}

void BooleanVar::set(bool value)
{
    set_var(value ? "1" : "0");
}

bool BooleanVar::get() const
{
    return get_var() == "1";
}

void BooleanVar::trace(std::function<void(const bool&)> callback)
{
    trace_var([callback](const std::string& v){
        callback(v == "1");
    });
}

IntVar::IntVar(const Widget& parent)
    : Var(parent, "int")
{
    set_var("0");
}

void IntVar::set(const int& value)
{
    set_var(std::to_string(value));
}

int IntVar::get() const
{
    return safe_stol(get_var().c_str());
}

void IntVar::trace(std::function<void(const int&)> callback)
{
    trace_var(callback);
}

DoubleVar::DoubleVar(const Widget& parent)
    : Var(parent, "double")
{
    set_var("0.0");
}

void DoubleVar::set(const double& value)
{
    set_var(std::to_string(value));
}

double DoubleVar::get() const
{
    return safe_stod(get_var().c_str());
}

void DoubleVar::trace(std::function<void(const double&)> callback)
{
    trace_var(callback);
}

Widget::Widget()
{}

Widget::Widget(const Widget& parent, const std::string &type, const std::string& name)
{
    impl_->interp = parent.impl_->interp;
    auto parent_name = (parent.full_name() == ".") ? "" : parent.full_name();
    impl_->full_name = parent_name + "." + (name.empty() ? type : name) + id;
    call({type, impl_->full_name});
}

const std::string& Widget::full_name() const
{
    return impl_->full_name;
}

std::string Widget::call(const std::vector<ArgValue>& words, bool* success) const
{
    if (impl_->interp == nullptr)
    {
        std::cerr << "cpp_tk Error: call() called on an uninitialized widget (interp == nullptr)." << std::endl;
        if (success) *success = false;
        return {};
    }
    return impl_->interp->call(words, success);
}

void Widget::register_void_callback(const std::string& name, std::function<void()> callback) const
{
    if (impl_->interp == nullptr)
    {
        std::cerr << "cpp_tk Error: register_void_callback() called on an uninitialized widget (interp == nullptr)." << std::endl;
        return;
    }
    impl_->interp->register_void_callback(name, callback);
}

void Widget::register_string_callback(const std::string& name, std::function<void(const std::string&)> callback) const
{
    if (impl_->interp == nullptr)
    {
        std::cerr << "cpp_tk Error: register_string_callback() called on an uninitialized widget (interp == nullptr)." << std::endl;
        return;
    }
    impl_->interp->register_string_callback(name, callback);
}

void Widget::register_double_callback(const std::string& name, std::function<void(const double&)> callback) const
{
    if (impl_->interp == nullptr)
    {
        std::cerr << "cpp_tk Error: register_double_callback() called on an uninitialized widget (interp == nullptr)." << std::endl;
        return;
    }
    impl_->interp->register_double_callback(name, callback);
}

void Widget::register_event_callback(const std::string& name, std::function<void(const Event&)> callback) const
{
    if (impl_->interp == nullptr)
    {
        std::cerr << "cpp_tk Error: register_event_callback() called on an uninitialized widget (interp == nullptr)." << std::endl;
        return;
    }
    impl_->interp->register_event_callback(name, callback);
}

void Widget::register_bool_callback(const std::string& name, std::function<bool(const std::string&)> callback) const
{
    if (impl_->interp == nullptr)
    {
        std::cerr << "cpp_tk Error: register_bool_callback() called on an uninitialized widget (interp == nullptr)." << std::endl;
        return;
    }
    impl_->interp->register_bool_callback(name, callback);
}

Widget& Widget::pack(const std::map<std::string, ArgValue> &options)
{
    std::vector<ArgValue> words = {"pack", impl_->full_name};
    for (const auto& kv : options)
    {
        words.push_back("-" + kv.first);
        words.push_back(kv.second);
    }
    call(words);
    return *this;
}

Widget& Widget::pack_forget()
{
    call({"pack", "forget", impl_->full_name});
    return *this;
}

Widget& Widget::grid(const std::map<std::string, ArgValue>& options)
{
    std::vector<ArgValue> words = {"grid", impl_->full_name};
    for (const auto& kv : options)
    {
        words.push_back("-" + kv.first);
        words.push_back(kv.second);
    }
    call(words);
    return *this;
}

Widget& Widget::grid_forget()
{
    call({"grid", "forget", impl_->full_name});
    return *this;
}

Widget& Widget::place(const std::map<std::string, ArgValue> &options)
{
    std::vector<ArgValue> words = {"place", impl_->full_name};
    for (const auto& kv : options)
    {
        words.push_back("-" + kv.first);
        words.push_back(kv.second);
    }
    call(words);
    return *this;
}

Widget& Widget::place_forget()
{
    call({"place", "forget", impl_->full_name});
    return *this;
}

Widget& Widget::config(const std::map<std::string, ArgValue> &options)
{
    if (options.empty())
    {
        return *this;
    }

    std::vector<ArgValue> words = {impl_->full_name, "configure"};
    for (const auto &kv : options)
    {
        words.push_back("-" + kv.first);
        words.push_back(kv.second);
    }
    call(words);
    return *this;
}

Widget& Widget::config(const std::string& name, const ArgValue& value)
{
    config({{name, value}});
    return *this;
}

Widget& Widget::grid_rowconfigure(int row, const std::map<std::string, ArgValue>& options)
{
    std::vector<ArgValue> words = {"grid", "rowconfigure", impl_->full_name, row};
    for (const auto& kv : options)
    {
        words.push_back("-" + kv.first);
        words.push_back(kv.second);
    }
    call(words);
    return *this;
}

Widget& Widget::grid_columnconfigure(int column, const std::map<std::string, ArgValue>& options)
{
    std::vector<ArgValue> words = {"grid", "columnconfigure", impl_->full_name, column};
    for (const auto& kv : options)
    {
        words.push_back("-" + kv.first);
        words.push_back(kv.second);
    }
    call(words);
    return *this;
}

std::string Widget::cget(const std::string& name) const
{
    auto ok  = false;
    auto ret = call({impl_->full_name, "cget", "-" + name}, &ok);
    if (!ok)
        ret = "";
    return ret;
}

Widget& Widget::bind(const std::string& event, std::function<void(const Event&)> callback)
{
    auto cb_name = sanitize(full_name()) + "_" + sanitize(event) + "_bind_cb";
    register_event_callback(cb_name, callback);
    std::string script = cb_name + " %x %y %X %Y %W %K %k %c %t %D";
    call({"bind", impl_->full_name, event, script});
    return *this;
}

Widget& Widget::unbind(const std::string& event)
{
    call({"bind", impl_->full_name, event, ""});
    return *this;
}

std::string Widget::after(const int& ms, std::function<void()> callback)
{
    auto cb_name = sanitize(full_name()) + "_after_cb_" + std::to_string(impl_->after_id++);
    register_void_callback(cb_name, callback);
    auto ok  = false;
    auto ret = call({"after", ms, cb_name}, &ok);
    return ret;
}

void Widget::after_idle(std::function<void()> callback)
{
    auto cb_name = sanitize(full_name()) + "_after_idle_cb";
    register_void_callback(cb_name, callback);
    call({"after", "idle", cb_name});
}

void Widget::after_cancel(const std::string& id)
{
    call({"after", "cancel", id});
}

void Widget::destroy()
{
    call({"destroy", impl_->full_name});
}

void Widget::update()
{
    call({"update"});
}

void Widget::update_idletasks()
{
    call({"update", "idletasks"});
}

void Widget::event_generate(const std::string& event, const std::map<std::string, ArgValue>& options)
{
    std::vector<ArgValue> words = {"event", "generate", impl_->full_name, event};
    for (const auto& kv : options)
    {
        words.push_back("-" + kv.first);
        words.push_back(kv.second);
    }
    call(words);
}

int Widget::winfo_width() const
{
    auto ret = call({"winfo", "width", impl_->full_name});
    return safe_stol(ret.c_str());
}

int Widget::winfo_height() const
{
    auto ret = call({"winfo", "height", impl_->full_name});
    return safe_stol(ret.c_str());
}

int Widget::winfo_x() const
{
    auto ret = call({"winfo", "x", impl_->full_name});
    return safe_stol(ret.c_str());
}

int Widget::winfo_y() const
{
    auto ret = call({"winfo", "y", impl_->full_name});
    return safe_stol(ret.c_str());
}

int Widget::winfo_rootx() const
{
    auto ret = call({"winfo", "rootx", impl_->full_name});
    return safe_stol(ret.c_str());
}

int Widget::winfo_rooty() const
{
    auto ret = call({"winfo", "rooty", impl_->full_name});
    return safe_stol(ret.c_str());
}

bool Widget::winfo_exists() const
{
    auto ret = call({"winfo", "exists", impl_->full_name});
    return safe_stol(ret.c_str()) != 0;
}

std::string Widget::winfo_class() const
{
    return call({"winfo", "class", impl_->full_name});
}

std::string Widget::winfo_toplevel() const
{
    return call({"winfo", "toplevel", impl_->full_name});
}

std::vector<std::string> Widget::winfo_children() const
{
    auto result = call({"winfo", "children", impl_->full_name});
    return impl_->interp->split_list(result);
}

int Widget::winfo_screenwidth() const
{
    auto ret = call({"winfo", "screenwidth", impl_->full_name});
    return safe_stol(ret.c_str());
}

int Widget::winfo_screenheight() const
{
    auto ret = call({"winfo", "screenheight", impl_->full_name});
    return safe_stol(ret.c_str());
}

int Widget::winfo_pointerx() const
{
    auto ret = call({"winfo", "pointerx", impl_->full_name});
    return safe_stol(ret.c_str());
}

int Widget::winfo_pointery() const
{
    auto ret = call({"winfo", "pointery", impl_->full_name});
    return safe_stol(ret.c_str());
}

std::string Widget::winfo_manager() const
{
    return call({"winfo", "manager", impl_->full_name});
}

bool Widget::winfo_ismapped() const
{
    auto ret = call({"winfo", "ismapped", impl_->full_name});
    return safe_stol(ret.c_str()) != 0;
}

Widget& Widget::focus_set()
{
    call({"focus", impl_->full_name});
    return *this;
}

Widget& Widget::focus_force()
{
    call({"focus", "-force", impl_->full_name});
    return *this;
}

std::string Widget::focus_get() const
{
    auto ok  = false;
    auto ret = call({"focus"}, &ok);
    return ok ? ret : "";
}

void Widget::clipboard_clear()
{
    call({"clipboard", "clear"});
}

void Widget::clipboard_append(const std::string& text)
{
    call({"clipboard", "append", text});
}

std::string Widget::clipboard_get() const
{
    auto ok  = false;
    auto ret = call({"clipboard", "get"}, &ok);
    return ok ? ret : "";
}

void Widget::wait_window() const
{
    call({"tkwait", "window", impl_->full_name});
}

void Widget::wait_variable(const Var& var) const
{
    call({"tkwait", "variable", var.name()});
}

Widget& Widget::lift()
{
    call({"raise", impl_->full_name});
    return *this;
}

Widget& Widget::lower()
{
    call({"lower", impl_->full_name});
    return *this;
}

PhotoImage::PhotoImage()
    : interp_(nullptr)
{}

PhotoImage::PhotoImage(const Widget& parent, const std::map<std::string, ArgValue>& options)
    : interp_(current_interp())
    , name_("img_" + id)
{
    std::vector<ArgValue> words = {"image", "create", "photo", name_};
    for (const auto& kv : options)
    {
        words.push_back("-" + kv.first);
        words.push_back(kv.second);
    }

    call(words);
}

const std::string& PhotoImage::name() const
{
    return name_;
}

std::string PhotoImage::call(const std::vector<ArgValue>& words, bool* success) const
{
    if (interp_ == nullptr)
    {
        std::cerr << "cpp_tk Error: call() called on an uninitialized PhotoImage (interp_ == nullptr)." << std::endl;
        if (success) *success = false;
        return {};
    }
    return interp_->call(words, success);
}

Tk::Tk()
    : Widget()
{
    impl_->interp = new Interpreter();
    interp_map[std::this_thread::get_id()] = impl_->interp;
    impl_->full_name = ".";

    title("tk");
    geometry("300x300");
    auto self_handle = handle();
    protocol("WM_DELETE_WINDOW", [self_handle](){ Tk(self_handle).quit(); });
}

Tk& Tk::title(const std::string& title)
{
    call({"wm", "title", ".", title});
    return *this;
}

Tk& Tk::geometry(const std::string &size)
{
    call({"wm", "geometry", ".", size});
    return *this;
}

std::string Tk::geometry() const
{
    return call({"wm", "geometry", "."});
}

Tk& Tk::protocol(const std::string& name, std::function<void()> handler) 
{
    auto cb_name = "protocol_cb_" + sanitize(name);
    register_void_callback(cb_name, handler);
    call({"wm", "protocol", ".", name, cb_name});
    return *this;
}

Tk& Tk::resizable(bool width, bool height)
{
    call({"wm", "resizable", ".", (int)(width ? 1 : 0), (int)(height ? 1 : 0)});
    return *this;
}

Tk& Tk::minsize(int width, int height)
{
    call({"wm", "minsize", ".", width, height});
    return *this;
}

Tk& Tk::maxsize(int width, int height)
{
    call({"wm", "maxsize", ".", width, height});
    return *this;
}

Tk& Tk::iconify()
{
    call({"wm", "iconify", "."});
    return *this;
}

Tk& Tk::deiconify()
{
    call({"wm", "deiconify", "."});
    return *this;
}

Tk& Tk::withdraw()
{
    call({"wm", "withdraw", "."});
    return *this;
}

Tk& Tk::state(const std::string& new_state)
{
    call({"wm", "state", ".", new_state});
    return *this;
}

std::string Tk::state() const
{
    return call({"wm", "state", "."});
}

Tk& Tk::attributes(const std::string& name, const std::string& value)
{
    call({"wm", "attributes", ".", name, value});
    return *this;
}

std::string Tk::attributes(const std::string& name) const
{
    return call({"wm", "attributes", ".", name});
}

Tk& Tk::lift()
{
    call({"raise", "."});
    return *this;
}

Tk& Tk::lower()
{
    call({"lower", "."});
    return *this;
}

Tk& Tk::grab_set()
{
    call({"grab", "set", "."});
    return *this;
}

Tk& Tk::grab_release()
{
    call({"grab", "release", "."});
    return *this;
}

Tk& Tk::iconphoto(const std::string& image_name)
{
    call({"wm", "iconphoto", ".", "-default", image_name});
    return *this;
}

Tk& Tk::iconbitmap(const std::string& bitmap_path)
{
    call({"wm", "iconbitmap", ".", bitmap_path});
    return *this;
}

void Tk::mainloop() 
{
    call({"vwait", "forever"});
}

void Tk::quit() 
{
    call({"set", "forever", 1});
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
    register_void_callback(cb, callback);
    config({{"command", cb}});
    return *this;
}

std::string Checkbutton::invoke()
{
    return Widget::call({impl_->full_name, "invoke"});
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
    call({"grid", "propagate", impl_->full_name, (int)(value ? 1 : 0)});
    return *this;
}

Toplevel::Toplevel(const Widget& parent, const std::map<std::string, ArgValue>& options)
    : Widget(parent, "toplevel")
{
    config(options);
    auto self_handle = handle();
    protocol("WM_DELETE_WINDOW", [self_handle](){ Widget(self_handle).destroy(); });
}

Toplevel& Toplevel::title(const std::string &title_text)
{
    call({"wm", "title", impl_->full_name, title_text});
    return *this;
}

Toplevel& Toplevel::geometry(const std::string &size)
{
    call({"wm", "geometry", impl_->full_name, size});
    return *this;
}

std::string Toplevel::geometry() const
{
    return call({"wm", "geometry", impl_->full_name});
}

Toplevel& Toplevel::protocol(const std::string& name, std::function<void()> handler) 
{
    std::string callback_name = "protocol_cb_" + sanitize(full_name()) + "_" + sanitize(name);
    register_void_callback(callback_name, handler);
    call({"wm", "protocol", impl_->full_name, name, callback_name});
    return *this;
}

Toplevel& Toplevel::resizable(bool width, bool height)
{
    call({"wm", "resizable", impl_->full_name, (int)(width ? 1 : 0), (int)(height ? 1 : 0)});
    return *this;
}

Toplevel& Toplevel::minsize(int width, int height)
{
    call({"wm", "minsize", impl_->full_name, width, height});
    return *this;
}

Toplevel& Toplevel::maxsize(int width, int height)
{
    call({"wm", "maxsize", impl_->full_name, width, height});
    return *this;
}

Toplevel& Toplevel::attributes(const std::string& name, const std::string& value)
{
    call({"wm", "attributes", impl_->full_name, name, value});
    return *this;
}

std::string Toplevel::attributes(const std::string& name) const
{
    return call({"wm", "attributes", impl_->full_name, name});
}

Toplevel& Toplevel::iconify()
{
    call({"wm", "iconify", impl_->full_name});
    return *this;
}

Toplevel& Toplevel::deiconify()
{
    call({"wm", "deiconify", impl_->full_name});
    return *this;
}

Toplevel& Toplevel::withdraw()
{
    call({"wm", "withdraw", impl_->full_name});
    return *this;
}

Toplevel& Toplevel::state(const std::string& new_state)
{
    call({"wm", "state", impl_->full_name, new_state});
    return *this;
}

std::string Toplevel::state() const
{
    return call({"wm", "state", impl_->full_name});
}

Toplevel& Toplevel::lift()
{
    call({"raise", impl_->full_name});
    return *this;
}

Toplevel& Toplevel::lower()
{
    call({"lower", impl_->full_name});
    return *this;
}

Toplevel& Toplevel::grab_set()
{
    call({"grab", "set", impl_->full_name});
    return *this;
}

Toplevel& Toplevel::grab_release()
{
    call({"grab", "release", impl_->full_name});
    return *this;
}

Toplevel& Toplevel::iconphoto(const std::string& image_name)
{
    call({"wm", "iconphoto", impl_->full_name, "-default", image_name});
    return *this;
}

Toplevel& Toplevel::iconbitmap(const std::string& bitmap_path)
{
    call({"wm", "iconbitmap", impl_->full_name, bitmap_path});
    return *this;
}

Toplevel& Toplevel::transient(const Widget& master)
{
    call({"wm", "transient", impl_->full_name, master.full_name()});
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
    register_void_callback(callback_name, callback);
    config({{"command", callback_name}});
    return *this;
}

std::string Button::invoke()
{
    return Widget::call({impl_->full_name, "invoke"});
}

Canvas::Canvas(const Widget& parent, const std::map<std::string, ArgValue>& options)
    : Widget(parent, "canvas", "c")
{
    config(options);
}

Canvas& Canvas::itemconfig(const std::string& id_or_tag, const std::map<std::string, ArgValue>& options)
{
    std::vector<ArgValue> words = {impl_->full_name, "itemconfigure", id_or_tag};
    for (const auto &kv : options)
    {
        words.push_back("-" + kv.first);
        words.push_back(kv.second);
    }
    call(words);
    return *this;
}

std::string Canvas::itemcget(const std::string& id_or_tag, const std::string& option) const
{
    auto ok  = false;
    auto ret = call({impl_->full_name, "itemcget", id_or_tag, "-" + option}, &ok);
    return ok ? ret : "";
}

std::vector<int> Canvas::bbox(const std::string& id_or_tag) const
{
    auto ok     = false;
    auto result = call({impl_->full_name, "bbox", id_or_tag}, &ok);

    std::vector<int> box;
    if (!ok)
        return box;

    for (const auto& token : impl_->interp->split_list(result))
        box.push_back(safe_stol(token.c_str()));
    return box;
}

Canvas& Canvas::tag_bind(const std::string& id_or_tag, const std::string& event, std::function<void(const Event&)> callback)
{
    auto cb_name = sanitize(full_name()) + "_tag_" + sanitize(id_or_tag) + "_" + sanitize(event);
    register_event_callback(cb_name, callback);
    std::string script = cb_name + " %x %y %X %Y %W %K %k %c %t %D";
    call({impl_->full_name, "bind", id_or_tag, event, script});
    return *this;
}

Canvas& Canvas::tag_unbind(const std::string& id_or_tag, const std::string& event)
{
    call({impl_->full_name, "bind", id_or_tag, event, ""});
    return *this;
}

std::string Canvas::create_line(const std::vector<std::array<double, 2>>& points, const std::map<std::string, ArgValue>& options)
{
    std::vector<ArgValue> words = {impl_->full_name, "create", "line"};
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
    return call(words);
}

std::string Canvas::create_line(const int& x1, const int& y1, const int& x2, const int& y2, const std::map<std::string, ArgValue>& options)
{
    std::vector<ArgValue> words = {impl_->full_name, "create", "line", x1, y1, x2, y2};
    for (const auto &kv : options)
    {
        words.push_back("-" + kv.first);
        words.push_back(kv.second);
    }
    return call(words);
}

std::string Canvas::create_oval(const int& x1, const int& y1, const int& x2, const int& y2, const std::map<std::string, ArgValue>& options)
{
    std::vector<ArgValue> words = {impl_->full_name, "create", "oval", x1, y1, x2, y2};
    for (const auto &kv : options)
    {
        words.push_back("-" + kv.first);
        words.push_back(kv.second);
    }
    return call(words);
}

std::string Canvas::create_rectangle(const int& x1, const int& y1, const int& x2, const int& y2, const std::map<std::string, ArgValue>& options)
{
    std::vector<ArgValue> words = {impl_->full_name, "create", "rectangle", x1, y1, x2, y2};
    for (const auto &kv : options)
    {
        words.push_back("-" + kv.first);
        words.push_back(kv.second);
    }
    return call(words);
}

std::string Canvas::create_text(const int& x, const int& y, const std::map<std::string, ArgValue>& options)
{
    std::vector<ArgValue> words = {impl_->full_name, "create", "text", x, y};
    for (const auto &kv : options)
    {
        words.push_back("-" + kv.first);
        words.push_back(kv.second);
    }
    return call(words);
}

std::string Canvas::create_polygon(const std::vector<int>& coords, const std::map<std::string, ArgValue>& options)
{
    std::vector<ArgValue> words = {impl_->full_name, "create", "polygon"};
    for (int c : coords)
        words.push_back(c);
    for (const auto& kv : options)
    {
        words.push_back("-" + kv.first);
        words.push_back(kv.second);
    }
    return call(words);
}

std::string Canvas::create_arc(int x1, int y1, int x2, int y2, const std::map<std::string, ArgValue>& options)
{
    std::vector<ArgValue> words = {impl_->full_name, "create", "arc", x1, y1, x2, y2};
    for (const auto& kv : options)
    {
        words.push_back("-" + kv.first);
        words.push_back(kv.second);
    }
    return call(words);
}

std::string Canvas::create_image(int x, int y, const std::map<std::string, ArgValue>& options)
{
    std::vector<ArgValue> words = {impl_->full_name, "create", "image", x, y};
    for (const auto& kv : options)
    {
        words.push_back("-" + kv.first);
        words.push_back(kv.second);
    }
    return call(words);
}

std::string Canvas::create_window(int x, int y, const Widget& widget, const std::map<std::string, ArgValue>& options)
{
    std::vector<ArgValue> words = {impl_->full_name, "create", "window", x, y, "-window", widget.full_name()};
    for (const auto& kv : options)
    {
        words.push_back("-" + kv.first);
        words.push_back(kv.second);
    }
    return call(words);
}

std::vector<std::string> Canvas::find_overlapping(int x1, int y1, int x2, int y2) const
{
    auto result = call({impl_->full_name, "find", "overlapping", x1, y1, x2, y2});
    return impl_->interp->split_list(result);
}

std::vector<std::string> Canvas::find_closest(int x, int y) const
{
    auto result = call({impl_->full_name, "find", "closest", x, y});
    return impl_->interp->split_list(result);
}

Canvas& Canvas::addtag(const std::string& tag, const std::string& where, const std::string& target)
{
    call({impl_->full_name, "addtag", tag, where, target});
    return *this;
}

Canvas& Canvas::dtag(const std::string& tag, const std::string& target)
{
    call({impl_->full_name, "dtag", target, tag});
    return *this;
}

std::vector<std::string> Canvas::gettags(const std::string& id) const
{
    auto result = call({impl_->full_name, "gettags", id});
    return impl_->interp->split_list(result);
}

Canvas& Canvas::coords(const std::string& item_id, const std::vector<int>& coords)
{
    std::vector<ArgValue> words = {impl_->full_name, "coords", item_id};
    for (int c : coords)
        words.push_back(c);
    call(words);
    return *this;
}

Canvas& Canvas::move(const std::string& id_or_tag, const int& x, const int& y)
{
    call({impl_->full_name, "move", id_or_tag, x, y});
    return *this;
}

Canvas& Canvas::moveto(const std::string& id_or_tag, const int& x, const int& y)
{
    call({impl_->full_name, "moveto", id_or_tag, x, y});
    return *this;
}

Canvas& Canvas::xview(const std::string& args)
{
    std::vector<ArgValue> words = {impl_->full_name, "xview"};
    for (const auto& token : impl_->interp->split_list(args))
        words.emplace_back(token);
    call(words);
    return *this;
}

Canvas& Canvas::yview(const std::string& args)
{
    std::vector<ArgValue> words = {impl_->full_name, "yview"};
    for (const auto& token : impl_->interp->split_list(args))
        words.emplace_back(token);
    call(words);
    return *this;
}

Canvas& Canvas::xscrollcommand(std::function<void(std::string)> callback)
{
    auto cb_name = sanitize(impl_->full_name) + "_xstring_cb";
    register_string_callback(cb_name, callback);
    config({{"xscrollcommand", cb_name}});
    return *this;
}

Canvas& Canvas::yscrollcommand(std::function<void(std::string)> callback)
{
    auto cb_name = sanitize(impl_->full_name) + "_ystring_cb";
    register_string_callback(cb_name, callback);
    config({{"yscrollcommand", cb_name}});
    return *this;
}

Canvas& Canvas::scale(const std::string& id_or_tag, const int& x, const int& y, const double& xscale, const double& yscale)
{
    call({impl_->full_name, "scale", id_or_tag, x, y, xscale, yscale});
    return *this;
}

Canvas& Canvas::erase(const std::string& id_or_tag)
{
    call({impl_->full_name, "delete", id_or_tag});
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
    call({impl_->full_name, "icursor", index});
    return *this;
}

Entry& Entry::insert(const std::string& index, const std::string& text) 
{
    call({impl_->full_name, "insert", index, text});
    return *this;
}

int Entry::index(const std::string& index) const 
{
    auto ok  = false;
    auto ret = call({impl_->full_name, "index", index}, &ok);
    if (!ok)
        return -1;
    return std::stol(ret);
}

Entry& Entry::erase(const std::string& start, const std::string& end) 
{
    std::vector<ArgValue> words = {impl_->full_name, "delete", start};
    if (!end.empty())
        words.push_back(end);
    call(words);
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
    auto ret = call({impl_->full_name, "get"}, &ok);
    return ret;
}

Entry& Entry::validate(const std::string& mode, std::function<bool(const std::string&)> callback)
{
    auto cb_name = sanitize(full_name()) + "_validate_cb";
    register_bool_callback(cb_name, callback);
    config({{"validate", mode}, {"validatecommand", cb_name + " %P"}});
    return *this;
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

LabelFrame::LabelFrame(const Widget& parent, const std::map<std::string, ArgValue>& options)
    : Widget(parent, "labelframe", "labelframe")
{
    config(options);
}

LabelFrame& LabelFrame::width(const int& width)
{
    config({{"width", std::to_string(width)}});
    return *this;
}

LabelFrame& LabelFrame::height(const int& height)
{
    config({{"height", std::to_string(height)}});
    return *this;
}

LabelFrame& LabelFrame::text(const std::string& text)
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
    call({impl_->full_name, "insert", index, item});
    return *this;
}

Listbox& Listbox::insert(const std::string& index, const std::string& item)
{
    call({impl_->full_name, "insert", index, item});
    return *this;
}

Listbox& Listbox::erase(int start, int end)
{
    call({impl_->full_name, "delete", start, end});
    return *this;
}

Listbox& Listbox::erase(const std::string& start, const std::string& end)
{
    std::vector<ArgValue> words = {impl_->full_name, "delete", start};
    if (!end.empty())
        words.push_back(end);
    call(words);
    return *this;
}

std::vector<int> Listbox::curselection() const
{
    std::string result = call({impl_->full_name, "curselection"});
    std::vector<int> indices;
    for (const auto& token : impl_->interp->split_list(result))
        indices.push_back(safe_stol(token.c_str()));
    return indices;
}

std::string Listbox::get(int index) const
{
    return call({impl_->full_name, "get", index});
}

std::string Listbox::get(const std::string& index) const
{
    return call({impl_->full_name, "get", index});
}

Listbox& Listbox::yscrollcommand(std::function<void(std::string)> callback)
{
    auto cb_name = sanitize(full_name()) + "_ystring_cb";
    register_string_callback(cb_name, callback);
    config({{"yscrollcommand", cb_name}});
    return *this;
}

Listbox& Listbox::selectmode(const std::string& mode)
{
    config({{"selectmode", mode}}); // "single", "browse", "multiple", "extended"
    return *this;
}

Listbox& Listbox::see(int index)
{
    call({impl_->full_name, "see", index});
    return *this;
}

Listbox& Listbox::see(const std::string& index)
{
    call({impl_->full_name, "see", index});
    return *this;
}

int Listbox::nearest(int y) const
{
    auto ret = call({impl_->full_name, "nearest", y});
    return safe_stol(ret.c_str());
}

int Listbox::size() const
{
    auto ret = call({impl_->full_name, "size"});
    return safe_stol(ret.c_str());
}

Listbox& Listbox::select_set(int first, int last)
{
    std::vector<ArgValue> words = {impl_->full_name, "selection", "set", first};
    if (last >= 0)
        words.push_back(last);
    call(words);
    return *this;
}

Listbox& Listbox::select_set(const std::string& first, const std::string& last)
{
    std::vector<ArgValue> words = {impl_->full_name, "selection", "set", first};
    if (!last.empty())
        words.push_back(last);
    call(words);
    return *this;
}

Listbox& Listbox::select_clear(int first, int last)
{
    std::vector<ArgValue> words = {impl_->full_name, "selection", "clear", first};
    if (last >= 0)
        words.push_back(last);
    call(words);
    return *this;
}

Listbox& Listbox::select_clear(const std::string& first, const std::string& last)
{
    std::vector<ArgValue> words = {impl_->full_name, "selection", "clear", first};
    if (!last.empty())
        words.push_back(last);
    call(words);
    return *this;
}

bool Listbox::select_includes(int index) const
{
    auto ret = call({impl_->full_name, "selection", "includes", index});
    return safe_stol(ret.c_str()) != 0;
}

bool Listbox::select_includes(const std::string& index) const
{
    auto ret = call({impl_->full_name, "selection", "includes", index});
    return safe_stol(ret.c_str()) != 0;
}

Listbox& Listbox::activate(int index)
{
    call({impl_->full_name, "activate", index});
    return *this;
}

Listbox& Listbox::activate(const std::string& index)
{
    call({impl_->full_name, "activate", index});
    return *this;
}

Menu::Menu(const Widget& parent, const std::map<std::string, ArgValue>& options)
    : Widget(parent, "menu", "menu")
{
    config(options);
}

Menu& Menu::add_command(const std::map<std::string, ArgValue>& options)
{
    std::vector<ArgValue> words = {impl_->full_name, "add", "command"};
    for (const auto& kv : options)
    {
        words.push_back("-" + kv.first);
        words.push_back(kv.second);
    }
    call(words);
    return *this;
}

Menu& Menu::add_cascade(const std::map<std::string, ArgValue>& options)
{
    std::vector<ArgValue> words = {impl_->full_name, "add", "cascade"};
    for (const auto& kv : options)
    {
        words.push_back("-" + kv.first);
        words.push_back(kv.second);
    }
    call(words);
    return *this;
}

Menu& Menu::add_separator()
{
    call({impl_->full_name, "add", "separator"});
    return *this;
}

Menu& Menu::add_checkbutton(const std::map<std::string, ArgValue>& options)
{
    std::vector<ArgValue> words = {impl_->full_name, "add", "checkbutton"};
    for (const auto& kv : options)
    {
        words.push_back("-" + kv.first);
        words.push_back(kv.second);
    }
    call(words);
    return *this;
}

Menu& Menu::add_radiobutton(const std::map<std::string, ArgValue>& options)
{
    std::vector<ArgValue> words = {impl_->full_name, "add", "radiobutton"};
    for (const auto& kv : options)
    {
        words.push_back("-" + kv.first);
        words.push_back(kv.second);
    }
    call(words);
    return *this;
}

Menu& Menu::insert(const std::string& index, const std::string& item_type, const std::map<std::string, ArgValue>& options)
{
    std::vector<ArgValue> words = {impl_->full_name, "insert", index, item_type};
    for (const auto& kv : options)
    {
        words.push_back("-" + kv.first);
        words.push_back(kv.second);
    }
    call(words);
    return *this;
}

Menu& Menu::entryconfigure(const std::string& index, const std::map<std::string, ArgValue>& options)
{
    std::vector<ArgValue> words = {impl_->full_name, "entryconfigure", index};
    for (const auto& kv : options)
    {
        words.push_back("-" + kv.first);
        words.push_back(kv.second);
    }
    call(words);
    return *this;
}

int Menu::index(const std::string& pattern) const
{
    auto ret = call({impl_->full_name, "index", pattern});
    if (ret == "none")
        return -1;
    return safe_stol(ret.c_str());
}

Menu& Menu::erase(const std::string& index)
{
    call({impl_->full_name, "delete", index});
    return *this;
}

Menu& Menu::post(int x, int y)
{
    call({impl_->full_name, "post", x, y});
    return *this;
}

Menu& Menu::unpost()
{
    call({impl_->full_name, "unpost"});
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

OptionMenu::OptionMenu(const Widget& parent, StringVar& variable, const std::vector<std::string>& values)
    : Widget(parent, "menubutton", "optmenu")
    , command_(std::make_shared<std::function<void(const std::string&)>>())
{
    menu_ = Menu(*this, {});
    for (std::size_t i = 0; i < values.size(); ++i)
    {
        std::string value = values[i];
        auto cb_name = sanitize(full_name()) + "_optmenu_cb_" + std::to_string(i);
        auto* var_ptr = &variable;
        auto cmd = command_;
        register_void_callback(cb_name, [var_ptr, value, cmd]() {
            var_ptr->set(value);
            if (*cmd) (*cmd)(value);
        });
        menu_.add_command({{"label", value}, {"command", cb_name}});
    }
    config({{"menu", menu_.full_name()}, {"textvariable", variable.name()}});
    if (!values.empty())
        variable.set(values.front());
}

OptionMenu& OptionMenu::command(std::function<void(const std::string&)> callback)
{
    *command_ = callback;
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
    std::vector<ArgValue> words = {impl_->full_name, "add", child.full_name()};
    for (const auto& kv : options)
    {
        words.push_back("-" + kv.first);
        words.push_back(kv.second);
    }
    call(words);
    return *this;
}

PanedWindow& PanedWindow::forget(const Widget& child)
{
    call({impl_->full_name, "forget", child.full_name()});
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
    call({impl_->full_name, "configure", "-value", val});
    return *this;
}

Radiobutton& Radiobutton::command(std::function<void()> callback)
{
    auto cb = sanitize(full_name()) + "_rb_cb";
    register_void_callback(cb, callback);
    config({{"command", cb}});
    return *this;
}

std::string Radiobutton::invoke()
{
    return Widget::call({impl_->full_name, "invoke"});
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
    register_double_callback(callback_name, callback);
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
    register_string_callback(callback_name, callback);
    config({{"command", callback_name}});
    return *this;
}

Scrollbar& Scrollbar::set(const std::string& args) 
{
    std::vector<ArgValue> words = {impl_->full_name, "set"};
    for (const auto& token : impl_->interp->split_list(args))
        words.emplace_back(token);
    call(words);
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
    register_void_callback(cb, callback);
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
    call({impl_->full_name, "insert", index, text});
    return *this;
}

std::string Text::get(const std::string& start, const std::string& end) const 
{
    return call({impl_->full_name, "get", start, end});
}

Text& Text::erase(const std::string& start, const std::string& end) 
{
    call({impl_->full_name, "delete", start, end});
    return *this;
}

Text& Text::yscrollcommand(std::function<void(std::string)> callback) 
{
    auto cb_name = sanitize(full_name()) + "_string_cb";
    register_string_callback(cb_name, callback);        
    config({{"yscrollcommand", cb_name}});
    return *this;
}

Text& Text::yview(const std::string& args)
{
    std::vector<ArgValue> words = {impl_->full_name, "yview"};
    for (const auto& token : impl_->interp->split_list(args))
        words.emplace_back(token);
    call(words);
    return *this;
}

Text& Text::wrap(const std::string& mode) 
{
    config({{"wrap", mode}});
    return *this;
}

Text& Text::tag_add(const std::string& tag, const std::string& start, const std::string& end)
{
    call({impl_->full_name, "tag", "add", tag, start, end});
    return *this;
}

Text& Text::tag_remove(const std::string& tag, const std::string& start, const std::string& end)
{
    call({impl_->full_name, "tag", "remove", tag, start, end});
    return *this;
}

Text& Text::tag_config(const std::string& tag, const std::map<std::string, ArgValue>& options)
{
    std::vector<ArgValue> words = {impl_->full_name, "tag", "configure", tag};
    for (const auto& kv : options)
    {
        words.push_back("-" + kv.first);
        words.push_back(kv.second);
    }
    call(words);
    return *this;
}

Text& Text::mark_set(const std::string& mark, const std::string& index)
{
    call({impl_->full_name, "mark", "set", mark, index});
    return *this;
}

Text& Text::mark_unset(const std::string& mark)
{
    call({impl_->full_name, "mark", "unset", mark});
    return *this;
}

std::string Text::search(const std::string& pattern, const std::string& index, const std::map<std::string, ArgValue>& options)
{
    std::vector<ArgValue> words = {impl_->full_name, "search"};
    for (const auto& kv : options)
    {
        words.push_back("-" + kv.first);
        words.push_back(kv.second);
    }
    words.push_back(pattern);
    words.push_back(index);
    auto ok  = false;
    auto ret = call(words, &ok);
    return ok ? ret : "";
}

Text& Text::see(const std::string& index)
{
    call({impl_->full_name, "see", index});
    return *this;
}

bool Text::compare(const std::string& index1, const std::string& op, const std::string& index2) const
{
    auto ret = call({impl_->full_name, "compare", index1, op, index2});
    return safe_stol(ret.c_str()) != 0;
}

int Text::count(const std::string& index1, const std::string& index2, const std::string& option) const
{
    auto ret = call({impl_->full_name, "count", "-" + option, index1, index2});
    return safe_stol(ret.c_str());
}

std::string Text::dump(const std::string& index1, const std::string& index2) const
{
    std::vector<ArgValue> words = {impl_->full_name, "dump", index1};
    if (!index2.empty())
        words.push_back(index2);
    return call(words);
}

std::string Text::image_create(const std::string& index, const std::map<std::string, ArgValue>& options)
{
    std::vector<ArgValue> words = {impl_->full_name, "image", "create", index};
    for (const auto& kv : options)
    {
        words.push_back("-" + kv.first);
        words.push_back(kv.second);
    }
    return call(words);
}

Text& Text::window_create(const std::string& index, const Widget& window, const std::map<std::string, ArgValue>& options)
{
    std::vector<ArgValue> words = {impl_->full_name, "window", "create", index, "-window", window.full_name()};
    for (const auto& kv : options)
    {
        words.push_back("-" + kv.first);
        words.push_back(kv.second);
    }
    call(words);
    return *this;
}

namespace font
{

Font::Font()
    : interp_(nullptr)
{}

Font::Font(const Widget& parent, const std::map<std::string, ArgValue>& option,
           const std::string& name, bool exists)
    : name_(name.empty() ? ("font_" + id) : name)
    , interp_(current_interp())
{
    if (!exists)
    {
        call({"font", "create", name_});
    }
    if (!option.empty())
    {
        config(option);
    }
}

Font nametofont(const Widget& parent, const std::string& name)
{
    return Font(parent, {}, name, /*exists=*/true);
}

Font& Font::config(const std::map<std::string, ArgValue>& option)
{
    std::vector<ArgValue> words = {"font", "configure", name_};
    for (const auto &kv : option)
    {
        words.push_back("-" + kv.first);
        words.push_back(kv.second);
    }
    call(words);
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

std::string Font::actual(const std::string& option) const
{
    return call({"font", "actual", name_, "-" + option});
}

std::string Font::metrics(const std::string& option) const
{
    return call({"font", "metrics", name_, "-" + option});
}

int Font::measure(const std::string& text) const
{
    auto ret = call({"font", "measure", name_, text});
    return safe_stol(ret.c_str());
}

const std::string& Font::name() const
{
    return name_;
}

std::string Font::call(const std::vector<ArgValue>& words, bool* success) const
{
    if (interp_ == nullptr)
    {
        std::cerr << "cpp_tk Error: call() called on an uninitialized Font (interp_ == nullptr)." << std::endl;
        if (success) *success = false;
        return {};
    }
    return interp_->call(words, success);
}

} // font

namespace ttk
{

Style::Style(const Widget& parent)
    : interp_(current_interp())
{}

Style& Style::configure(const std::string& style_name, const std::map<std::string, ArgValue>& options)
{
    std::vector<ArgValue> words = {"ttk::style", "configure", style_name};
    for (const auto& kv : options)
    {
        words.push_back("-" + kv.first);
        words.push_back(kv.second);
    }
    call(words);
    return *this;
}

Style& Style::map(const std::string& style_name, const std::map<std::string, std::vector<std::string>>& options)
{
    std::vector<ArgValue> words = {"ttk::style", "map", style_name};
    for (const auto& kv : options)
    {
        words.push_back("-" + kv.first);
        words.push_back(kv.second);
    }
    call(words);
    return *this;
}

std::string Style::lookup(const std::string& style_name, const std::string& option) const
{
    return call({"ttk::style", "lookup", style_name, "-" + option});
}

std::vector<std::string> Style::theme_names() const
{
    return interp_->split_list(call({"ttk::style", "theme", "names"}));
}

Style& Style::theme_use(const std::string& theme_name)
{
    // Python本家もttk::styleではなくttk::setThemeを使う($ttk::currentThemeの追従のため)
    call({"ttk::setTheme", theme_name});
    return *this;
}

std::string Style::theme_use() const
{
    return call({"ttk::style", "theme", "use"});
}

std::string Style::call(const std::vector<ArgValue>& words, bool* success) const
{
    if (interp_ == nullptr)
    {
        std::cerr << "cpp_tk Error: call() called on an uninitialized Style (interp_ == nullptr)." << std::endl;
        if (success) *success = false;
        return {};
    }
    return interp_->call(words, success);
}

Button::Button(const Widget& parent, const std::map<std::string, ArgValue>& options)
    : Widget(parent, "ttk::button", "ttk_b")
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
    register_void_callback(callback_name, callback);
    config({{"command", callback_name}});        
    return *this;
}

Button& Button::font(const font::Font& font)
{
    config({{"font", font.name()}});
    return *this;
}

std::string Button::invoke()
{
    return Widget::call({impl_->full_name, "invoke"});
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
    register_void_callback(cb, callback);
    config({{"command", cb}});
    return *this;
}

std::string Checkbutton::invoke()
{
    return Widget::call({impl_->full_name, "invoke"});
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
    // ArgValue(vector<string>)がTcl_NewListObj経由で要素ごとに正しくquoteするため、
    // 手組みの "{" + item + "}" 文字列連結(itemが"}"を含むと壊れる)は不要。
    config({{"values", items}});
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

Combobox& Combobox::font(const font::Font& font)
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
    call({impl_->full_name, "insert", index, text});
    return *this;
}

std::string Combobox::get() const 
{
    return call({impl_->full_name, "get"});
}

Combobox& Combobox::erase(const std::string& start, const std::string& end) 
{
    call({impl_->full_name, "delete", start, end});
    return *this;
}

int Combobox::current() const
{
    const auto val = call({impl_->full_name, "current"});
    return safe_stol(val.c_str());
}

Combobox& Combobox::current(const int& idx)
{
    call({impl_->full_name, "current", idx});
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
    call({impl_->full_name, "icursor", index});
    return *this;
}

Entry& Entry::insert(const std::string& index, const std::string& text) 
{
    call({impl_->full_name, "insert", index, text});
    return *this;
}

int Entry::index(const std::string& index) const 
{
    auto ok  = false;
    auto ret = call({impl_->full_name, "index", index}, &ok);
    if (!ok)
        return -1;
    return std::stol(ret);
}

Entry& Entry::erase(const std::string& start, const std::string& end) 
{
    std::vector<ArgValue> words = {impl_->full_name, "delete", start};
    if (!end.empty())
        words.push_back(end);
    call(words);
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
    auto ret = call({impl_->full_name, "get"}, &ok);
    return ret;
}

Entry& Entry::validate(const std::string& mode, std::function<bool(const std::string&)> callback)
{
    auto cb_name = sanitize(full_name()) + "_validate_cb";
    register_bool_callback(cb_name, callback);
    config({{"validate", mode}, {"validatecommand", cb_name + " %P"}});
    return *this;
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

Label& Label::font(const font::Font& font)
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
    call({impl_->full_name, "add", child.full_name(), "-text", label});
    return *this;
}

Notebook& Notebook::select(int index) 
{
    call({impl_->full_name, "select", index});
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
    call({impl_->full_name, "start", interval});
    return *this;
}

Progressbar& Progressbar::stop()
{
    call({impl_->full_name, "stop"});
    return *this;
}

Progressbar& Progressbar::step(double amount)
{
    call({impl_->full_name, "step", amount});
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
    call({impl_->full_name, "configure", "-value", val});
    return *this;
}

Radiobutton& Radiobutton::command(std::function<void()> callback)
{
    auto cb = sanitize(full_name()) + "_rb_cb";
    register_void_callback(cb, callback);
    config({{"command", cb}});
    return *this;
}

std::string Radiobutton::invoke()
{
    return Widget::call({impl_->full_name, "invoke"});
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
    register_double_callback(callback_name, callback);
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
    register_string_callback(callback_name, callback);
    config({{"command", callback_name}});
    return *this;
}

Scrollbar& Scrollbar::set(const std::string& args)
{
    std::vector<ArgValue> words = {impl_->full_name, "set"};
    for (const auto& token : impl_->interp->split_list(args))
        words.emplace_back(token);
    call(words);
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
    register_void_callback(cb, callback);
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
    std::vector<ArgValue> words = {impl_->full_name, "insert", parent, index, "-id", iid};
    for (const auto& kv : options)
    {
        words.push_back("-" + kv.first);
        words.push_back(kv.second);
    }
    call(words);
    return *this;
}

Treeview& Treeview::erase(const std::string& iid)
{
    call({impl_->full_name, "delete", iid});
    return *this;
}

Treeview& Treeview::item(const std::string& iid, const std::map<std::string, ArgValue>& options)
{
    if (options.empty())
    {
        // getter 的な使い方をしたい場合は、必要に応じて別メソッドを追加してもよい
        call({impl_->full_name, "item", iid});
        return *this;
    }

    std::vector<ArgValue> words = {impl_->full_name, "item", iid};
    for (const auto& kv : options)
    {
        words.push_back("-" + kv.first);
        words.push_back(kv.second);
    }
    call(words);
    return *this;
}

Treeview& Treeview::heading(const std::string& column, const std::map<std::string, ArgValue>& options)
{
    std::vector<ArgValue> words = {impl_->full_name, "heading", column};
    for (const auto& kv : options)
    {
        words.push_back("-" + kv.first);
        words.push_back(kv.second);
    }
    call(words);
    return *this;
}

Treeview& Treeview::column(const std::string& column, const std::map<std::string, ArgValue>& options)
{
    std::vector<ArgValue> words = {impl_->full_name, "column", column};
    for (const auto& kv : options)
    {
        words.push_back("-" + kv.first);
        words.push_back(kv.second);
    }
    call(words);
    return *this;
}

std::vector<std::string> Treeview::selection() const
{
    auto ret = call({impl_->full_name, "selection"});
    return impl_->interp->split_list(ret);
}

Treeview& Treeview::selection_set(const std::vector<std::string>& iids)
{
    std::vector<ArgValue> words = {impl_->full_name, "selection", "set"};
    for (const auto& iid : iids)
        words.push_back(iid);
    call(words);
    return *this;
}

Treeview& Treeview::selection_add(const std::vector<std::string>& iids)
{
    std::vector<ArgValue> words = {impl_->full_name, "selection", "add"};
    for (const auto& iid : iids)
        words.push_back(iid);
    call(words);
    return *this;
}

Treeview& Treeview::selection_remove(const std::vector<std::string>& iids)
{
    std::vector<ArgValue> words = {impl_->full_name, "selection", "remove"};
    for (const auto& iid : iids)
        words.push_back(iid);
    call(words);
    return *this;
}

Treeview& Treeview::selection_toggle(const std::vector<std::string>& iids)
{
    std::vector<ArgValue> words = {impl_->full_name, "selection", "toggle"};
    for (const auto& iid : iids)
        words.push_back(iid);
    call(words);
    return *this;
}

bool Treeview::exists(const std::string& iid) const
{
    auto ret = call({impl_->full_name, "exists", iid});
    return safe_stol(ret.c_str()) != 0;
}

Treeview& Treeview::see(const std::string& iid)
{
    call({impl_->full_name, "see", iid});
    return *this;
}

Treeview& Treeview::xview(const std::string& args)
{
    std::vector<ArgValue> words = {impl_->full_name, "xview"};
    for (const auto& token : impl_->interp->split_list(args))
        words.emplace_back(token);
    call(words);
    return *this;
}

Treeview& Treeview::yview(const std::string& args)
{
    std::vector<ArgValue> words = {impl_->full_name, "yview"};
    for (const auto& token : impl_->interp->split_list(args))
        words.emplace_back(token);
    call(words);
    return *this;
}

Treeview& Treeview::xscrollcommand(std::function<void(std::string)> callback)
{
    auto cb_name = sanitize(impl_->full_name) + "_xstring_cb";
    register_string_callback(cb_name, callback);
    config({{"xscrollcommand", cb_name}});
    return *this;
}

Treeview& Treeview::yscrollcommand(std::function<void(std::string)> callback)
{
    auto cb_name = sanitize(impl_->full_name) + "_ystring_cb";
    register_string_callback(cb_name, callback);
    config({{"yscrollcommand", cb_name}});
    return *this;
}

Treeview& Treeview::set(const std::string& iid, const std::string& column, const ArgValue& value)
{
    call({impl_->full_name, "set", iid, column, value});
    return *this;
}

std::string Treeview::set(const std::string& iid, const std::string& column) const
{
    return call({impl_->full_name, "set", iid, column});
}

Treeview& Treeview::move(const std::string& iid, const std::string& parent, const std::string& index)
{
    call({impl_->full_name, "move", iid, parent, index});
    return *this;
}

Treeview& Treeview::detach(const std::string& iid)
{
    call({impl_->full_name, "detach", iid});
    return *this;
}

Treeview& Treeview::reattach(const std::string& iid, const std::string& parent, const std::string& index)
{
    call({impl_->full_name, "reattach", iid, parent, index});
    return *this;
}

std::vector<std::string> Treeview::get_children(const std::string& iid) const
{
    auto ret = call({impl_->full_name, "children", iid});
    return impl_->interp->split_list(ret);
}

std::string Treeview::parent(const std::string& iid) const
{
    return call({impl_->full_name, "parent", iid});
}

int Treeview::index(const std::string& iid) const
{
    auto ret = call({impl_->full_name, "index", iid});
    return std::stoi(ret);
}

Treeview& Treeview::focus(const std::string& iid)
{
    call({impl_->full_name, "focus", iid});
    return *this;
}

std::string Treeview::focus() const
{
    return call({impl_->full_name, "focus"});
}

Treeview& Treeview::tag_configure(const std::string& tag, const std::map<std::string, ArgValue>& options)
{
    std::vector<ArgValue> words = {impl_->full_name, "tag", "configure", tag};
    for (const auto& kv : options)
    {
        words.push_back("-" + kv.first);
        words.push_back(kv.second);
    }
    call(words);
    return *this;
}

Treeview& Treeview::tag_bind(const std::string& tag, const std::string& event, std::function<void(const Event&)> callback)
{
    auto cb_name = sanitize(full_name()) + "_tag_" + sanitize(tag) + "_" + sanitize(event);

    register_event_callback(cb_name, callback);

    std::string script = cb_name + " %x %y %X %Y %W %K %k %c %t %D";

    call({impl_->full_name, "tag", "bind", tag, event, script});
    return *this;
}

std::string Treeview::identify_row(int y) const
{
    return call({impl_->full_name, "identify", "row", y});
}

std::string Treeview::identify_column(int x) const
{
    return call({impl_->full_name, "identify", "column", x});
}

std::vector<int> Treeview::bbox(const std::string& iid, const std::string& column) const
{
    auto ret = call({impl_->full_name, "bbox", iid, column});
    std::vector<int> out;
    for (const auto& token : impl_->interp->split_list(ret))
        out.push_back(safe_stol(token.c_str()));
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
    auto ret = interp->call(words, &ok);
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
    return interp->call(words);
}

std::string askopenfile(const std::map<std::string, ArgValue>& options)
{
    return invoke_dialog("tk_getOpenFile", options);
}

std::vector<std::string> askopenfilenames(const std::map<std::string, ArgValue>& options)
{
    auto* interp = interp_map[std::this_thread::get_id()];
    if (!interp) return {};

    std::vector<ArgValue> words = {"tk_getOpenFile", "-multiple", 1};
    for (const auto& kv : options)
    {
        words.push_back("-" + kv.first);
        words.push_back(kv.second);
    }
    // 戻り値はTclのリスト形式(要素にスペースを含む場合は{}で囲まれる)のため、
    // 単純な空白splitではなくTcl_SplitListで正しく要素分解する。
    return interp->split_list(interp->call(words));
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
    return interp->call({"tk_messageBox", "-type", type, "-icon", icon, "-title", title, "-message", message});
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

bool askyesnocancel(const std::string& title, const std::string& message)
{
    return msgbox("yesnocancel", "question", title, message) == "yes";
}

} // messagebox

} // cpp_tk