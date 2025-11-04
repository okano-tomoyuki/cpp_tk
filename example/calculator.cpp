/**
 * @file calculator.cpp
 * @author okano tomoyuki (okano.development@gmail.com)
 * @brief 簡易電卓アプリケーションのサンプル
 * 
 * このサンプルはC++でGUIアプリケーションを作成するためのライブラリ「cpp_tk」を使用しています。
 * GUI部分のコードはPythonのTkinterに似た構造を持ち、直感的に理解しやすい設計となっています。
 * この電卓アプリケーションは基本的な四則演算（加算、減算、乗算、除算）をサポートしており、
 * ユーザーがボタンをクリックすることで操作できます。
 * 計算結果はリアルタイムで表示され、シンプルながらも実用的な電卓機能を提供します。
 * このサンプルコードを通じて、cpp_tkライブラリの使い方やC++でのGUIアプリケーション開発の基礎を学ぶことができます。
 * ぜひ参考にして、自分だけのGUIアプリケーションを作成してみてください！
 * 
 * @version 0.1
 * @date 2025-11-05
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#include <string>
#include <sstream>
#include <functional>
#include "cpp_tk.hpp"

int main()
{
    namespace tk    = cpp_tk;
    namespace ttk   = tk::ttk;

    auto app = tk::Tk();
    app.title("Calculator");
    app.geometry("400x500");

    std::string item1;
    std::string operation;
    bool op_clicked = false;

    auto label_result = ttk::Label(&app);
    label_result
        .text("0")
        .config({
            {"bg", "white"},
            {"anchor", "e"},
            {"font", "Arial 30"},
            {"width", "20"},
            {"height", "2"}
        })
        .grid({{"column", "0"}, {"row", "0"}, {"columnspan", "4"}, {"sticky", "nsew"}});

    auto create_button = [&app](const std::string &text, const int &col, const int &row, const int &colspawn, std::function<void()> on_click)
    {
        auto btn = ttk::Button(&app);
        btn
            .text(text)
            .command(on_click)
            .config({{"font", "Arial 30"}})
            .grid({{"column", std::to_string(col)}, {"row", std::to_string(row)}, {"columnspan", std::to_string(colspawn)}, {"sticky", "nsew"}});
    };

    // ACボタン
    create_button("AC", 0, 1, 3, [&](){
        label_result.text("0");
        item1.clear();
        operation.clear();
        op_clicked = false; 
    });

    // 数字ボタン
    auto numbers = std::vector<std::string>{"7", "8", "9", "4", "5", "6", "1", "2", "3", "0"};
    for (size_t i = 0; i < numbers.size(); ++i)
    {
        int x = i % 3;
        int y = i / 3 + 2;
        if (numbers[i] == "0")
        {
            create_button("0", 0, 5, 3, [&](){
                auto current = label_result.cget("text");
                if (op_clicked || current == "0") 
                {
                    label_result.text("0");
                } 
                else 
                {
                    label_result.text(current + "0");
                }
                op_clicked = false; 
            });
        }
        else
        {
            auto num = numbers[i];
            create_button(numbers[i], x, y, 1, [&, num](){
                auto current = label_result.cget("text");
                if (op_clicked || current == "0") 
                {
                    label_result.text(num);
                } 
                else 
                {
                    label_result.text(current + num);
                }
                op_clicked = false; 
            });
        }
    }

    // 演算子ボタン
    auto operators = std::vector<std::string>{"÷", "*", "-", "+", "="};
    for (size_t i = 0; i < operators.size(); ++i)
    {
        auto op = operators[i];
        create_button(operators[i], 3, i + 1, 1, [&, op](){
            auto current = label_result.cget("text");
            double num = 0;
            try 
            {
                num = std::stod(current);
            } 
            catch (...) 
            {
                num = 0;
            }

            if (op == "=") 
            {
                if (!item1.empty() && !operation.empty() && !op_clicked) 
                {
                    double lhs = std::stod(item1);
                    double rhs = num;
                    double result = 0;
                    if (operation == "+") result = lhs + rhs;
                    else if (operation == "-") result = lhs - rhs;
                    else if (operation == "*") result = lhs * rhs;
                    else if (operation == "÷") result = (rhs != 0) ? lhs / rhs : 0;
                    label_result.text(std::to_string(result));
                    item1.clear();
                    operation.clear();
                }

                op_clicked = true;
                return;
            }

            if (item1.empty()) 
            {
                item1 = current;
            } 
            else if (!op_clicked) 
            {
                double lhs = std::stod(item1);
                double rhs = num;
                double result = 0;
                if (operation == "+") result = lhs + rhs;
                else if (operation == "-") result = lhs - rhs;
                else if (operation == "*") result = lhs * rhs;
                else if (operation == "÷") result = (rhs != 0) ? lhs / rhs : 0;
                label_result.text(std::to_string(result));
                item1 = std::to_string(result);
            }

            operation = op;
            op_clicked = true; 
        });
    }

    // グリッド調整
    for (int i = 0; i < 6; i++)
    {
        app.grid_rowconfigure(i, {{"weight", "1"}});
    }

    for (int i = 0; i < 4; i++)
    {
        app.grid_columnconfigure(i, {{"weight", "1"}});
    }

    app.mainloop();
    return 0;
}
