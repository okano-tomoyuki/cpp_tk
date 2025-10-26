#ifndef CPP_TK_HPP
#define CPP_TK_HPP

#include <tk.h>
#include <tcl.h>
#include <string>
#include <sstream>
#include <iostream>
#include <functional>

class Object
{

public:
    Object(const std::string &value)
    {
        obj_ = Tcl_NewStringObj(value.c_str(), -1);
        Tcl_IncrRefCount(obj_);
    }

    virtual ~Object()
    {
        Tcl_DecrRefCount(obj_);
    }

    Tcl_Obj *get() const
    {
        return obj_;
    }

    std::string str() const
    {
        return Tcl_GetString(obj_);
    }

protected:
    Tcl_Obj *obj_;
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
            // std::cerr << "Tcl Error: " << Tcl_GetStringResult(interp_) << std::endl;
            return false;
        }
        return true;
    }

    Tcl_Interp *get() const
    {
        return interp_;
    }

private:
    Tcl_Interp *interp_;
};

class Widget : public Object
{
public:
    Widget(Interpreter* interp, const std::string& type)
        : Object("." + type + std::to_string(reinterpret_cast<uintptr_t>(this)))
        , interp_(interp)
    {
        full_name_ = "." + type + std::to_string(reinterpret_cast<uintptr_t>(this));
    }

    Widget(Widget* parent, const std::string& type)
        : Object("." + type + std::to_string(reinterpret_cast<uintptr_t>(this)))
        , interp_(parent->interpreter())
    {
        full_name_ = parent->full_name() + "." + type + std::to_string(reinterpret_cast<uintptr_t>(this));
    }

    const std::string& full_name() const
    {
        return full_name_;
    }

    Interpreter* interpreter() const
    {
        return interp_;
    }

    Widget& pack(const std::string& options = "")
    {
        create();
        interp_->evaluate("pack " + full_name_ + " " + options);
        return *this;
    }

    Widget& grid(const std::string& options = "")
    {
        create();
        interp_->evaluate("grid " + full_name_ + " " + options);
        return *this;
    }

    Widget& place(const std::string& options = "")
    {
        create();
        interp_->evaluate("place " + full_name_ + " " + options);
        return *this;
    }

protected:
    virtual void create() = 0;

private:
    Interpreter*    interp_;
    std::string     full_name_;
};

class Frame : public Widget 
{
public:
    Frame(Interpreter* interp)
        : Widget(interp, "f") 
    {}

    Frame(Widget* parent)
        : Widget(parent, "f") 
    {}    

private:
    bool created = false;

    void create()
    {
        if (!created)
        {
            std::ostringstream oss;
            oss << "frame " << full_name();
            interpreter()->evaluate(oss.str());
            created = true;
        }
    }

};

class Toplevel : public Widget
{
public:
    Toplevel(Interpreter* interp)
        : Widget(interp, "toplevel")
    {
        create();
    }

    Toplevel& title(const std::string& title_text)
    {
        interpreter()->evaluate("wm title " + full_name() + " \"" + title_text + "\"");
        return *this;
    }

    Toplevel& geometry(const std::string& size)
    {
        interpreter()->evaluate("wm geometry " + full_name() + " " + size);
        return *this;
    }

    Toplevel& protocol(const std::string& event, const std::string& command)
    {
        interpreter()->evaluate("wm protocol " + full_name() + " " + event + " {" + command + "}");
        return *this;
    }

protected:
    void create() override
    {
        interpreter()->evaluate("toplevel " + full_name());
    }
};


class Button : public Widget
{

public:
    Button(Interpreter* interp)
        : Widget(interp, "b") 
    {}

    Button(Widget* parent)
        : Widget(parent, "b") 
    {}

    Button& text(const std::string &t)
    {
        text_ = t;
        return *this;
    }

    Button& command(std::function<void()> func)
    {
        static int callback_id = 0;
        callback_id++;

        std::string callback_name = "callback_" + std::to_string(callback_id);
        callback_map[callback_name] = func;

        Tcl_CreateCommand(interpreter()->get(), callback_name.c_str(), generic_callback, nullptr, nullptr);
        command_ = callback_name;

        return *this;
    }

private:
    std::string     name_;
    std::string     text_;
    std::string     command_;
    bool            created = false;

    static std::unordered_map<std::string, std::function<void()>> callback_map;

    void create() override
    {
        if (!created)
        {
            std::ostringstream oss;
            oss << "button " << full_name();
            if (!text_.empty())
                oss << " -text \"" << text_ << "\"";
            if (!command_.empty())
                oss << " -command " << command_;
            interpreter()->evaluate(oss.str());
            created = true;
        }
    }

    static int generic_callback(ClientData client_data, Tcl_Interp* interp, int argc, const char* argv[])
    {
        std::string key = argv[0];
        auto it = callback_map.find(key);
        if (it != callback_map.end())
        {
            it->second();
        }
        return TCL_OK;
    }

};

class Label : public Widget
{
public:
    Label(Interpreter* interp)
        : Widget(interp, "l") {}

    Label(Widget* parent)
        : Widget(parent, "l") {}

    Label& text(const std::string& t)
    {
        text_ = t;
        return *this;
    }

    Label& config()
    {
        std::string cmd = full_name() + " configure";
        if (!text_.empty())
        {
            cmd += " -text \"" + text_ + "\"";
        }
        interpreter()->evaluate(cmd);
        return *this;
    }

protected:
    void create() override
    {
        if (created_)
        {
            return;
        }

        std::string cmd = "label " + full_name();
        if (!text_.empty())
        {
            cmd += " -text \"" + text_ + "\"";
        }

        interpreter()->evaluate(cmd);
        created_ = true;
    }

private:
    std::string text_;
    bool created_ = false;
};

std::unordered_map<std::string, std::function<void()>> Button::callback_map;

#endif