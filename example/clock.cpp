#include <cmath>
#include <ctime>
#include <array>
#include "cpp_tk.hpp"

int main() 
{
    const auto WIDTH = 300;
    const auto HEIGHT = 300;
    const auto CENTER_X = WIDTH / 2;
    const auto CENTER_Y = HEIGHT / 2;
    const auto CLOCK_RADIUS = 100;
    const auto PI = 3.14159265358979323846;

    namespace tk = cpp_tk;

    auto app = tk::Tk();
    app.title("Clock");

    auto canvas = tk::Canvas(&app);
    canvas.width(WIDTH).height(HEIGHT);
    canvas.pack();

    // 時計の外枠
    canvas.create_oval(
        CENTER_X - CLOCK_RADIUS, CENTER_Y - CLOCK_RADIUS,
        CENTER_X + CLOCK_RADIUS, CENTER_Y + CLOCK_RADIUS,
        {{"outline", "black"}}
    );

    // 目盛りと数字の描画
    for (int i = 0; i < 12; i++) 
    {
        double angle = PI / 2 - i * PI / 6;
        int outer_x = CENTER_X + CLOCK_RADIUS * std::cos(angle);
        int outer_y = CENTER_Y - CLOCK_RADIUS * std::sin(angle);
        int inner_x = CENTER_X + (CLOCK_RADIUS - 10) * std::cos(angle);
        int inner_y = CENTER_Y - (CLOCK_RADIUS - 10) * std::sin(angle);

        canvas.create_line(inner_x, inner_y, outer_x, outer_y);

        int text_x = CENTER_X + (CLOCK_RADIUS - 20) * std::cos(angle);
        int text_y = CENTER_Y - (CLOCK_RADIUS - 20) * std::sin(angle);
        canvas.create_text(text_x, text_y, {{"text", "\"" + std::to_string(i == 0 ? 12 : i) + "\""}});
    }

    // 針のIDを保持
    auto hour_hand_id = canvas.create_line(CENTER_X, CENTER_Y, CENTER_X, CENTER_Y - 50, {{"width", "4"}, {"fill", "black"}});
    auto minute_hand_id = canvas.create_line(CENTER_X, CENTER_Y, CENTER_X, CENTER_Y - 70, {{"width", "2"}, {"fill", "blue"}});
    auto second_hand_id = canvas.create_line(CENTER_X, CENTER_Y, CENTER_X, CENTER_Y - 90, {{"width", "1"}, {"fill", "red"}});

    // 時計更新関数
    std::function<void()> update_clock = [&]() {
        std::time_t now = std::time(nullptr);
        std::tm* local = std::localtime(&now);

        int hour = local->tm_hour % 12;
        int minute = local->tm_min;
        int second = local->tm_sec;

        double hour_angle = PI / 2 - (hour + minute / 60.0) * PI / 6;
        double minute_angle = PI / 2 - minute * PI / 30;
        double second_angle = PI / 2 - second * PI / 30;

        auto calc_pos =  [](const double& angle, const double& length) {
            return std::array<int, 2>{
                static_cast<int>(CENTER_X + length * std::cos(angle)),
                static_cast<int>(CENTER_Y - length * std::sin(angle))
            };
        };

        auto hxy = calc_pos(hour_angle, 50);
        auto mxy = calc_pos(minute_angle, 70);
        auto sxy = calc_pos(second_angle, 90);

        canvas.coords(hour_hand_id, std::vector<int>{CENTER_X, CENTER_Y, hxy[0], hxy[1]});
        canvas.coords(minute_hand_id, std::vector<int>{CENTER_X, CENTER_Y, mxy[0], mxy[1]});
        canvas.coords(second_hand_id, std::vector<int>{CENTER_X, CENTER_Y, sxy[0], sxy[1]});

        app.after(1000, update_clock); // 1秒ごとに更新
    };

    update_clock(); // 初回呼び出し

    app.mainloop();
    return 0;
}