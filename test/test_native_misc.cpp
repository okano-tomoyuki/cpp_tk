// アプリケーション/ミスクレベルのネイティブTclコマンド(第4回棚卸し)の回帰テスト:
// windowingsystem/bell/wait_visibility/scaling/bindtags/image_names/image_types/
// PhotoImage・BitmapImage::destroy()
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "cpp_tk.hpp"

#include <thread>
#include <chrono>

namespace tk = cpp_tk;

TEST_CASE("windowingsystem: x11/win32/aquaのいずれかを返す")
{
    tk::Tk root;
    root.withdraw();

    auto ws = root.windowingsystem();
    CHECK((ws == "x11" || ws == "win32" || ws == "aqua"));
}

TEST_CASE("bell: 例外にならない")
{
    tk::Tk root;
    root.withdraw();
    CHECK_NOTHROW(root.bell());
}

TEST_CASE("wait_visibility: 戻った時点でmappedになっている")
{
    tk::Tk root;
    root.geometry("50x50-3000-3000"); // withdrawだとmappedにならないため画面外配置にする

    root.wait_visibility();
    CHECK(root.winfo_ismapped());
}

TEST_CASE("scaling: getter/setterが機能する")
{
    tk::Tk root;
    root.withdraw();

    auto original = root.scaling();
    CHECK(original > 0);

    root.scaling(1.5);
    // Tk内部でピクセル単位への丸め込みが入るため厳密な浮動小数点一致は期待できず、
    // 誤差を広めに許容する(cpp_tkの不具合ではなくTk自身の丸め仕様)。
    CHECK(root.scaling() == doctest::Approx(1.5).epsilon(0.01));

    root.scaling(original); // 他のテストに影響しないよう元に戻す
}

TEST_CASE("bindtags: getter/setterが機能する")
{
    tk::Tk root;
    root.withdraw();
    tk::Button btn(root);
    btn.pack();

    auto tags = btn.bindtags();
    CHECK(tags.size() == 4); // [self, class, toplevel, all] が既定

    std::vector<std::string> custom_tags = {btn.full_name(), "all"};
    btn.bindtags(custom_tags);
    CHECK(btn.bindtags() == custom_tags);
}

TEST_CASE("image_names/image_types: 生成した画像が一覧に現れ、destroy()で消える")
{
    tk::Tk root;
    root.withdraw();

    auto types = tk::image_types();
    bool has_photo = false, has_bitmap = false;
    for (auto& t : types) { if (t == "photo") has_photo = true; if (t == "bitmap") has_bitmap = true; }
    CHECK(has_photo);
    CHECK(has_bitmap);

    tk::PhotoImage img(root);
    auto names_before = tk::image_names();
    bool found = false;
    for (auto& n : names_before) if (n == img.name()) found = true;
    CHECK(found);

    img.destroy();
    auto names_after = tk::image_names();
    found = false;
    for (auto& n : names_after) if (n == img.name()) found = true;
    CHECK_FALSE(found);
}
