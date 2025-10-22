#ifndef TK_WIDGET_HPP
#define TK_WIDGET_HPP

#include "object.hpp"
#include "interpreter.hpp"

#include <utility>
#include <vector>
#include <tcl.h>

namespace Tk
{

class Widget
{
public:
    Widget(const Object &type, const Widget &parent, const Object &name, std::initializer_list<Object> options)
    {
        // if (parent.path_.get_u8string_view() == u8".")
        // {
        //     Tcl_Obj *objv[] = {name.get()};
        //     path_ = Tcl_Format(interp_.get(), ".%s", 1, objv);
        // }
        // else
        // {
        //     Tcl_Obj *objv[] = {parent.path_.get(), name.get()};
        //     path_ = Tcl_Format(interp_.get(), "%s.%s", 2, objv);
        // }

        // // コマンドの組み立て
        // std::vector<Tcl_Obj *> args{type.get(), path_.get()};
        // std::for_each(options.begin(), options.end(), [&args](auto option)
        //                 { args.push_back(option.get()); });
        // args.push_back(nullptr);
        // Tcl_Obj *command = Tcl_ConcatObj(args.size() - 1, args.data());
        // interp_.evaluate(command);
    }

    virtual ~Widget()
    {
    }

private:
    Interpreter interp_;
    Object path_;
};

class Label : public Widget
{

public:
    Label(const Widget &parent, const Object &name, std::initializer_list<Object> options)
        : Widget(type, parent, name, options)
    {
    }

    ~Label() = default;
    Label(const Label &) = default;
    Label(Label &&) = default;
    Label& operator=(const Label &) = default;
    Label& operator=(Label &&) = default;

private:
    static constexpr char type[16] = u8"label";

};

class Button : public Widget
{

public:
    Button(const Widget &parent, const Object &name, std::initializer_list<Object> options)
        : Widget(type, parent, name, options)
    {
    }

    ~Button() = default;
    Button(const Button &) = default;
    Button(Button &&) = default;
    Button& operator=(const Button &) = default;
    Button& operator=(Button &&) = default;

private:
    static constexpr char type[16] = u8"button";

};


} // Tk

#endif // TK_WIDGET_HPP