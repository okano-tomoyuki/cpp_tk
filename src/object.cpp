#include <utility>
#include <tcl.h>
#include "object.hpp"

using namespace Tk;

class Object::Impl
{
public:
    Tcl_Obj* obj;

    Impl() 
        : obj(Tcl_NewObj())
    {
        Tcl_IncrRefCount(obj);
    }

    Impl(const Impl &other)
        : obj(Tcl_DuplicateObj(other.obj))
    {
        Tcl_IncrRefCount(obj);
    }

    ~Impl()
    {
        Tcl_DecrRefCount(obj);
    }

};

Object::Object()
    : impl_(new Impl()) 
{}

Object::Object(const Object &other)
    : impl_(new Impl(*other.impl_)) 
{}

Object::Object(Object &&other)
    : impl_(other.impl_)
{
    other.impl_ = nullptr;
}

Object::~Object()
{
    delete impl_;
}

Object& Object::operator=(const Object &other)
{
    if (this != &other)
    {
        Object tmp(other);
        swap(tmp);
    }
    return *this;
}

Object& Object::operator=(Object&& other)
{
    if (this != &other)
    {
        delete impl_;
        impl_ = other.impl_;
        other.impl_ = nullptr;
    }
    return *this;
}

void Object::swap(Object& other)
{
    std::swap(impl_, other.impl_);
}