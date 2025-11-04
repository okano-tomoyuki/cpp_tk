/**
 * @file eyes.cpp
 * @author okano tomoyuki (okano.development@gmail.com)
 * @brief 目がマウスカーソルを追いかけるサンプル
 * 
 * このサンプルは以下のサイトのアイデアを参考にしています：
 * https://daeudaeu.com/eye_ball/
 * 
 * @version 0.1
 * @date 2025-11-04
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#include <cmath>
#include "cpp_tk.hpp"

int main() 
{
    namespace tk         = cpp_tk;

    // 目の描画パラメータ
    constexpr auto WHITE_A = 50;
    constexpr auto WHITE_B = 60;

    constexpr auto LEFT_X = 250;
    constexpr auto LEFT_Y = 140;

    constexpr auto RIGHT_X = 350;
    constexpr auto RIGHT_Y = 140;

    constexpr auto BLACK_R = 16;

    auto app    = tk::Tk();
    app
        .title("Eyes");

    auto canvas = tk::Canvas(&app);

    canvas
        .width(500)
        .height(500)
        .config({
            {"bg", "white"},
            {"highlightthickness", "0"}
        })
        .pack();

    // 白目
    canvas.create_oval(
        LEFT_X - WHITE_A, LEFT_Y - WHITE_B,
        LEFT_X + WHITE_A, LEFT_Y + WHITE_B,
        {{"fill", "white"}}
    );

    canvas.create_oval(
        RIGHT_X - WHITE_A, RIGHT_Y - WHITE_B,
        RIGHT_X + WHITE_A, RIGHT_Y + WHITE_B,
        {{"fill", "white"}}
    );

    // 黒目
    canvas.create_oval(
        LEFT_X - BLACK_R, LEFT_Y - BLACK_R,
        LEFT_X + BLACK_R, LEFT_Y + BLACK_R,
        {{"fill", "black"}, {"tags", "left_black"}}
    );

    canvas.create_oval(
        RIGHT_X - BLACK_R, RIGHT_Y - BLACK_R,
        RIGHT_X + BLACK_R, RIGHT_Y + BLACK_R,
        {{"fill", "black"}, {"tags", "right_black"}}
    );

    // マウス移動時のイベントを設定
    canvas.bind("<Motion>", [&](const tk::Event& event) {

        // 左の目の楕円のパラメータ算出
        auto x = event.x - LEFT_X;
        auto y = event.y - LEFT_Y;
        auto a = WHITE_A - BLACK_R;
        auto b = WHITE_B - BLACK_R;

        auto lx = 0;
        auto ly = 0;

        // 楕円方程式からマウスカーソルが白目の中にあるかを判断
        if ((x * x) / (a * a) + (y * y) / (b * b) < 1)
        {
            // 白目の中ならマウスカーソルの位置に黒目を描画
            lx = event.x;
            ly = event.y;
        }
        else
        {
            // 白目の外なら楕円の線上に黒目を描画
            auto rad    = std::atan2(y, x);
            lx     = LEFT_X + a * std::cos(rad);
            ly     = LEFT_Y + b * std::sin(rad);
        }

        // 右の目の楕円のパラメータ算出
        x = event.x - RIGHT_X;
        y = event.y - RIGHT_Y;
        a = WHITE_A - BLACK_R;
        b = WHITE_B - BLACK_R;

        auto rx = 0;
        auto ry = 0;

        // 楕円方程式からマウスカーソルが白目の中にあるかを判断
        if ((x * x) / (a * a) + (y * y) / (b * b) < 1)
        {
            // 白目の中ならマウスカーソルの位置に黒目を描画
            rx = event.x;
            ry = event.y;
        }
        else
        {
            // 白目の外なら楕円の線上に黒目を描画
            auto rad = std::atan2(y, x);
            rx = RIGHT_X + a * std::cos(rad);
            ry = RIGHT_Y + b * std::sin(rad);
        }

        // 求めた位置に左側の黒目の座標を移動
        canvas.coords(
            "left_black",
            {lx - BLACK_R, ly - BLACK_R,
            lx + BLACK_R, ly + BLACK_R}
        );

        // 求めた位置に右側の黒目の座標を設定
        canvas.coords(
            "right_black",
            {rx - BLACK_R, ry - BLACK_R,
            rx + BLACK_R, ry + BLACK_R}
        );
    });

    app.mainloop();

    return 0;
}