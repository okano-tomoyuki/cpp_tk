#ifndef TK_INTERPRETER_HPP
#define TK_INTERPRETER_HPP

namespace Tk
{

class Interpreter
{
public:
    Interpreter();
    Interpreter(const Interpreter&) = delete;
    Interpreter& operator=(const Interpreter&) = delete;
    Interpreter(Interpreter&& other);
    Interpreter& operator=(Interpreter&& other);
    ~Interpreter();

    void swap(Interpreter& other);

    // 拡張予定: コマンド実行などのAPI

private:
    class Impl;
    Impl* impl_;
};

} // Tk

#endif