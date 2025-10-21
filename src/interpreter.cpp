#include <tcl.h>
#include <utility>
#include "interpreter.hpp"

using namespace Tk;

class Interpreter::Impl
{
public:
    Impl()
    {
        interp = Tcl_CreateInterp();
    }

    ~Impl()
    {
        if (interp)
        {
            Tcl_DeleteInterp(interp);
        }
    }

    Tcl_Interp *interp;
};

Interpreter::Interpreter()
    : impl_(new Impl()) 
{}

Interpreter::Interpreter(Interpreter&& other)
    : impl_(other.impl_)
{
    other.impl_ = nullptr;
}

Interpreter& Interpreter::operator=(Interpreter&& other)
{
    if (this != &other)
    {
        delete impl_;
        impl_ = other.impl_;
        other.impl_ = nullptr;
    }
    return *this;
}

Interpreter::~Interpreter()
{
    delete impl_;
}

void Interpreter::swap(Interpreter& other)
{
    std::swap(impl_, other.impl_);
}
