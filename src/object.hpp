#ifndef TK_OBJECT_HPP
#define TK_OBJECT_HPP

namespace Tk
{

class Object
{
public:
    Object();
    Object(const Object& other);
    Object(Object&& other);
    Object(const char* type);
    ~Object();
    Object& operator=(const Object& other);
    Object& operator=(Object&& other);
    void swap(Object& other);

private:
    class Impl;
    Impl *impl_;
};

}

#endif