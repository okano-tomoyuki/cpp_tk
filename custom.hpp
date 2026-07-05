#ifndef CPP_TK_CUSTOM_HPP
#define CPP_TK_CUSTOM_HPP

#include "cpp_tk.hpp"

// 本家Python tkinterも実は一貫しておらず、tkinter.scrolledtext.ScrolledTextは別モジュール、
// tkinter.OptionMenuはcoreのtkinterモジュール、ttk.LabeledScaleはttkモジュール内、という
// ようにバラバラに配置されている。cpp_tkでは「単一のTclウィジェットとして代替不可能な合成」
// (OptionMenu)はcpp_tk namespace直下に残し、「自分で組めるが便利なだけの合成」
// (ScrolledText/LabeledScale)はこのcpp_tk::custom namespaceに切り出して、
// core(=個々のTclウィジェットと1:1対応する薄いラッパー)の純粋性を保つ。
namespace cpp_tk
{
namespace custom
{

/**
 * Text+Scrollbar(縦)を合成し、スクロール連携まで済ませた便利ウィジェット
 * (Python tkinter.scrolledtext.ScrolledText相当)。Text固有の操作(insert/tag_*等)は
 * text()経由で行う(本家はText由来のメソッドを全て自身に持つが、C++の静的型では
 * 全メソッドを1つずつ委譲すると保守コストが高いため、内部ウィジェットへの参照を
 * 公開する方式にしている)。
 */
class ScrolledText : public Widget
{
public:
    using Widget::Widget; // share<ScrolledText>()用にWidget(shared_ptr<Impl>)を継承する

    ScrolledText() = default;

    explicit ScrolledText(const Widget& parent, const std::map<std::string, ArgValue>& options = {});

    /** 内部のTextウィジェットへの参照。insert/get/tag_*等のText固有の操作はこちら経由で行う。 */
    Text& text();

    /** 内部のScrollbarウィジェットへの参照。 */
    Scrollbar& scrollbar();

private:
    Text      text_;
    Scrollbar scrollbar_;
};

/**
 * 現在値を表示するLabel+Scaleを合成したウィジェット(Python tkinter.ttk.LabeledScale相当)。
 * Tclネイティブの"ttk::labeledscale"ウィジェットは存在せず、本家Python実装もFrame+Label+Scale
 * の合成であるため、cpp_tkも同様の構成にしている。
 */
class LabeledScale : public Widget
{
public:
    using Widget::Widget; // share<LabeledScale>()用にWidget(shared_ptr<Impl>)を継承する

    LabeledScale() = default;

    /** variableは呼び出し側が生存期間を管理する(Checkbutton::variable等と同様の制約)。 */
    explicit LabeledScale(const Widget& parent, DoubleVar& variable, double from = 0, double to = 10);

    /** 内部のScaleウィジェットへの参照。 */
    ttk::Scale& scale();

    /** 内部のLabelウィジェットへの参照(値表示ラベル)。 */
    ttk::Label& label();

private:
    ttk::Scale scale_;
    ttk::Label label_;
};

/**
 * 月表示のカレンダーWidget(Python tkcalendarパッケージのCalendar相当のMVP版)。年/月/日は
 * intで表現し、外部の日付ライブラリには依存しない(内部の日数/曜日計算は<ctime>の
 * std::mktime()を利用する)。v1スコープは月移動・日付選択・get_date()のみで、mindate/maxdate
 * 制限やイベント表示・年選択は今後の課題とする(docs/tasks.md G節参照)。
 */
class Calendar : public Widget
{
public:
    using Widget::Widget; // コールバック内でhandle()から安全に再構築するために継承する(docs/tasks.md C節8.参照)

    Calendar() = default;

    /** year/month/dayを省略(いずれか0のまま)すると本日の日付で初期化する。 */
    explicit Calendar(const Widget& parent, int year = 0, int month = 0, int day = 0);

    /** 現在選択されている日付を取得する。 */
    void get_date(int& year, int& month, int& day) const;

    /** 選択日を変更する(表示月も選択日の月に合わせて切り替わる)。 */
    Calendar& set_date(int year, int month, int day);

    /** 日付選択時に呼ばれるコールバック(選択されたyear/month/dayを引数で受け取る)。 */
    Calendar& command(std::function<void(int, int, int)> callback);

    // 内部実装用(custom.cpp内のヘルパー関数から参照するためpublicだが、直接使用しない)。
    struct Model;

private:
    std::shared_ptr<Model> model_;
};

/**
 * Python tkinter.simpledialogに相当する、Entryを1つ持つ簡易モーダル入力ダイアログ群。
 * 本家simpledialogもTclネイティブなダイアログコマンドではなくToplevel+Label+Entry+Button
 * を組み立てるPure Python実装であるため、cpp_tkでもScrolledText等と同様にcustom名前空間に置く。
 */
namespace simpledialog
{

/**
 * 文字列入力を求めるモーダルダイアログを表示し、OK時の入力値を返す(Python simpledialog.askstring()相当)。
 * 本家はキャンセル時にNoneを返すが、cpp_tkはoptional相当を持たないため、cancelledが非nullptrなら
 * そこへキャンセル有無を書き込む(キャンセル時の戻り値は空文字列)。
 */
std::string askstring(const Widget& parent, const std::string& title, const std::string& prompt,
                       const std::string& initial_value = "", bool* cancelled = nullptr);

/**
 * 整数入力を求めるモーダルダイアログ(Python simpledialog.askinteger()相当)。askstring()の上に
 * 構築した簡略版のため、数値として解釈できない入力は本家のような再入力プロンプトは出さず、
 * キャンセル扱いにする。
 */
int askinteger(const Widget& parent, const std::string& title, const std::string& prompt,
               int initial_value = 0, bool* cancelled = nullptr);

/** 浮動小数点入力を求めるモーダルダイアログ(Python simpledialog.askfloat()相当)。askinteger()と同じ簡略化。 */
double askfloat(const Widget& parent, const std::string& title, const std::string& prompt,
                double initial_value = 0.0, bool* cancelled = nullptr);

} // simpledialog

/**
 * Sun Valley ttk theme(Windows 11風のモダンなttkテーマ、https://github.com/rdbende/Sun-Valley-ttk-theme
 * by rdbende、MITライセンス)を適用する。thirdparty/sv_ttk/にベンダリングしたTclスクリプト・
 * 画像アセット(LICENSE-sv_ttk.txtにライセンス原文を同梱)をビルド時にcpp_tk自身へ埋め込んでおり、
 * 呼び出し時にOS一時ディレクトリへ同じ相対構造で展開してから`source`する(cpp_tkは単一の
 * ライブラリをリンクするだけで完結する設計のため、別途アセットディレクトリを配布・指定する
 * 必要はない)。darkにfalseを指定するとライトテーマになる。
 */
void use_sv_ttk_theme(bool dark = true);

} // custom
} // cpp_tk

#endif // CPP_TK_CUSTOM_HPP
