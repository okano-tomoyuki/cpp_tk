#ifndef CPP_TTK_HPP
#define CPP_TTK_HPP

#include "cpp_tk.hpp"

namespace cpp_ttk
{

using Object        = cpp_tk::Object;
using Interpreter   = cpp_tk::Interpreter;
using Widget        = cpp_tk::Widget;
using StringVar     = cpp_tk::StringVar;

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

} // cpp_ttk

#endif // CPP_TTK_HPP