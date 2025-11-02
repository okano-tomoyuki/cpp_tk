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

    bool evaluate(const std::string &command)
    {
        int result = Tcl_Eval(interp_, command.c_str());
        if (result != TCL_OK)
        {
            std::cerr << "Tcl Error: " << Tcl_GetStringResult(interp_) << std::endl;
            return false;
        }
        return true;
    }

    void set_var(const std::string& name, const std::string& value);
    std::string get_var(const std::string& name);

    Tcl_Interp *get() const
    {
        return interp_;
    }

    void register_simple_callback(const std::string& name, std::function<void()> callback)
    {
        simple_callback_map_[name] = callback;

        Tcl_CreateCommand(interp_, name.c_str(), [](ClientData client_data, Tcl_Interp*, int argc, const char* argv[]) -> int {
            auto* self = static_cast<Interpreter*>(client_data);
            auto it = self->simple_callback_map_.find(argv[0]);
            if (it != self->simple_callback_map_.end()) 
            {
                it->second();
            }
            return TCL_OK;
        }, this, nullptr);        
    }

    void register_event_callback(const std::string& event, std::function<void(const Event&)> callback) 
    {
        event_callback_map_[event] = callback;

        Tcl_CreateCommand(interp_, event.c_str(), [](ClientData client_data, Tcl_Interp*, int argc, const char* argv[]) -> int {
            
            auto safe_stol = [](const char* s)
            {
                if (!s || *s == '\0' || std::strcmp(s, "??") == 0) 
                    return 0;

                char* endptr = nullptr;
                int ret = std::strtol(s, &endptr, 10);

                if (endptr == s || *endptr != '\0') 
                    return 0;

                return ret;
            };
            
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
    std::unordered_map<std::string, std::function<void(const Event&)>>  event_callback_map_;
    std::unordered_map<std::string, std::function<void()>>              simple_callback_map_;
    Tcl_Interp *interp_;
};

class Widget
{
public:

    Widget(Widget *parent, const std::string &type, const std::string& name="")
        : interp_(parent != nullptr ? parent->interpreter() : new Interpreter())
    {
        if (parent != nullptr)
        {
            full_name_ = parent->full_name() + "." + (name.empty() ? type : name) + std::to_string(reinterpret_cast<uintptr_t>(this));
            evaluate(type + " " + full_name_);
        }
        else
        {
            full_name_ = "";
        }
    }

    const std::string& full_name() const
    {
        return full_name_;
    }

    void pack(const std::string &options = "")
    {
        evaluate("pack " + full_name_ + " " + options);
    }

    void grid(const std::string &options = "")
    {
        evaluate("grid " + full_name_ + " " + options);
    }

    void place(const std::string &options = "")
    {
        evaluate("place " + full_name_ + " " + options);
    }

    void config(const std::map<std::string, std::string> &option)
    {
        std::ostringstream oss;
        oss << full_name() << " configure";
        for (const auto &kv : option)
        {
            oss << " -" << kv.first << " " << kv.second;
        }
        interpreter()->evaluate(oss.str());
    }

    void bind(const std::string& event, std::function<void(const Event&)> callback)
    {
        std::string callback_name = "bind_cb_" + sanitize(full_name()) + "_" + sanitize(event);
        interpreter()->register_event_callback(callback_name, callback);
        std::string bind_cmd = "bind " + full_name() + " " + event + " {" + callback_name + " %x %y %X %Y %W %K %k %c %t}";
        evaluate(bind_cmd);
    }

    void destroy()
    {
        interpreter()->evaluate("destroy " + full_name());
    }

    bool evaluate(const std::string& cmd)
    {
        return interp_->evaluate(cmd);
    }

protected:
    Interpreter *interpreter() const
    {
        return interp_;
    }

protected:
    Interpreter *interp_;
    std::string full_name_;

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
        evaluate("wm title . \"tk\"");
        evaluate("wm geometry . 300x300");
        evaluate("wm protocol . WM_DELETE_WINDOW {exit}");        
    }

    Tk& title(const std::string& title)
    {
        evaluate("wm title . \"" + title + "\"");
        return *this;
    }

    Tk& geometry(const std::string &size)
    {
        evaluate("wm geometry . " + size);
        return *this;
    }

    Tk& protocol(const std::string& name, std::function<void()> handler) 
    {
        auto cb_name = "protocol_cb_" + sanitize(name);
        interpreter()->register_simple_callback(cb_name, handler);
        evaluate("wm protocol . " + name + " " + cb_name);
        return *this;
    }

    void mainloop() 
    {
        evaluate("vwait forever");
    }

    void quit() 
    {
        evaluate("set forever 1");
    }
};

class Frame : public Widget
{
public:
    Frame(Widget *parent)
        : Widget(parent, "frame", "f")
    {}

    Frame &width(const int &width)
    {
        config({{"width", std::to_string(width)}});
        return *this;
    }

    Frame &height(const int &height)
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
        evaluate("wm title " + full_name() + " \"" + title_text + "\"");
        return *this;
    }

    Toplevel& geometry(const std::string &size)
    {
        evaluate("wm geometry " + full_name() + " " + size);
        return *this;
    }

    Toplevel& protocol(const std::string& name, std::function<void()> handler) 
    {
        std::string callback_name = "protocol_cb_" + sanitize(full_name()) + "_" + sanitize(name);
        interpreter()->register_simple_callback(callback_name, handler);
        evaluate("wm protocol " + full_name() + " " + name + " " + callback_name);
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
        std::string callback_name =  sanitize(full_name()) + "_simple_callback";
        interpreter()->register_simple_callback(callback_name, callback);
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
        interpreter()->evaluate(oss.str());
        return *this;
    }

    Canvas &create_rectangle(const int &left, const int &up, const int &right, const int &down)
    {
        std::ostringstream oss;
        oss << full_name() << " create" << " " << "rectangle" << " " << left << " " << up << " " << right << " " << down << " -fill red -outline green";
        interpreter()->evaluate(oss.str());
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
        interpreter()->evaluate(oss.str());
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

class StringVar
{
public:
    StringVar(Interpreter *interp, const std::string &name = "")
        : interp_(interp)
    {
        name_ = name.empty() ? "var_" + std::to_string(reinterpret_cast<uintptr_t>(this)) : name;
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
    {}

    Entry& textvariable(const StringVar &var)
    {
        config({{"textvariable", var.name()}});
        return *this;
    }

    std::string get() const
    {
        interpreter()->evaluate("set __entry_value [" + full_name() + " get]");
        return Tcl_GetVar(interpreter()->get(), "__entry_value", TCL_GLOBAL_ONLY);
    }
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

#endif