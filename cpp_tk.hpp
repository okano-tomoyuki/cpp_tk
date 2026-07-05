#ifndef CPP_TK_HPP
#define CPP_TK_HPP

#include <string>
#include <functional>
#include <map>
#include <memory>
#include <unordered_map>
#include <vector>
#include <array>
#include <cstdint>
#include <stdexcept>

namespace cpp_tk
{

static constexpr const char END[4]              = "end";        // 最後の位置
static constexpr const char INSERT[7]           = "insert";     // カーソル位置
static constexpr const char CURRENT[8]          = "current";    // マウスポインタ下の要素（Canvasなど）
static constexpr const char ANCHOR[7]           = "anchor";     // 選択の起点
static constexpr const char SEL_FIRST[10]       = "sel.first";  // 選択範囲の先頭
static constexpr const char SEL_LAST[9]         = "sel.last";   // 選択範囲の末尾
static constexpr const char ACTIVE[7]           = "active";     // アクティブな要素（Listboxなど）
static constexpr const char NONE[5]             = "none";       // 無効状態（stateなど）
static constexpr const char NORMAL[7]           = "normal";     // 通常状態
static constexpr const char DISABLED[9]         = "disabled";   // 無効状態
static constexpr const char READONLY[9]         = "readonly";   // 読み取り専用（Entryなど）

static constexpr const char N[2]                = "n";          // 北（上）
static constexpr const char S[2]                = "s";          // 南（下）
static constexpr const char E[2]                = "e";          // 東（右）
static constexpr const char W[2]                = "w";          // 西（左）

static constexpr const char NE[3]               = "ne";         // 北東（右上）
static constexpr const char NW[3]               = "nw";         // 北西（左上）
static constexpr const char SE[3]               = "se";         // 南東（右下）
static constexpr const char SW[3]               = "sw";         // 南西（左下）
static constexpr const char CENTER[7]           = "center";     // 中央

static constexpr const char LEFT[5]             = "left";       //
static constexpr const char RIGHT[6]            = "right";      //
static constexpr const char TOP[4]              = "top";        //
static constexpr const char BOTTOM[7]           = "bottom";     //

// for fill
static constexpr const char FILL_NONE[5]        = "none";
static constexpr const char FILL_X[2]           = "x";
static constexpr const char FILL_Y[2]           = "y";
static constexpr const char FILL_BOTH[5]        = "both";

// for expand
static constexpr const char EXPAND_TRUE[5]      = "true";
static constexpr const char EXPAND_FALSE[6]     = "false";

// for anchor
static constexpr const char ANCHOR_N[2]         = "n";
static constexpr const char ANCHOR_S[2]         = "s";
static constexpr const char ANCHOR_E[2]         = "e";
static constexpr const char ANCHOR_W[2]         = "w";
static constexpr const char ANCHOR_NE[3]        = "ne";
static constexpr const char ANCHOR_NW[3]        = "nw";
static constexpr const char ANCHOR_SE[3]        = "se";
static constexpr const char ANCHOR_SW[3]        = "sw";
static constexpr const char ANCHOR_CENTER[7]    = "center";

// for relief
static constexpr const char FLAT[5]             = "flat";
static constexpr const char RAISED[7]           = "raised";
static constexpr const char SUNKEN[7]           = "sunken";
static constexpr const char GROOVE[7]           = "groove";
static constexpr const char RIDGE[6]            = "ridge";

// for wrap
// static constexpr const char NONE[5]             = "none";
static constexpr const char CHAR[5]             = "char";
static constexpr const char WORD[5]             = "word";

// for orient
static constexpr const char HORIZONTAL[11]      = "horizontal";
static constexpr const char VERTICAL[9]         = "vertical";

struct Event
{
    int         x;
    int         y;
    int         x_root;
    int         y_root;
    std::string widget;
    std::string character;
    std::string keysym;
    int         keycode;
    std::string type;
    int         delta;
    
};

class Interpreter;
class Widget;
class Var;

class ArgValue
{
public:
    enum class ValueType
    {
        NONE,
        STRING,
        INT,
        DOUBLE,
        BOOL,
        BYTES,      // バイナリデータ (PhotoImage 等) 用
        STRING_LIST // Tclのリスト値(-filetypes等、要素ごとに個別quoteされたリストが必要な場合)用
    };

    ArgValue();

    ArgValue(const std::string& s);

    ArgValue(const char* s);

    ArgValue(int v);

    ArgValue(double v);

    ArgValue(bool v);

    ArgValue(const std::vector<uint8_t>& bytes);

    ArgValue(const std::vector<std::string>& list);

    /**
     * Var(StringVar等)を渡すとname()を取り出してSTRING扱いにする(Python本家のconfig(variable=var)相当の書き味)。
     * name()を読むだけで保持はしないが、Menu::add_checkbutton等のように、ここが"-variable"/"-textvariable"
     * 相当のオプションへVarを渡す唯一の経路になるケースがあるため、Checkbutton::variable()等と同じ理由で
     * 右辺値の一時変数を弾くためあえて非constの参照にしている。
     */
    ArgValue(Var& var);

    ArgValue(const ArgValue& other);

    ArgValue& operator=(const ArgValue& other);

    ~ArgValue();

    ValueType type() const;

    // 内部 Tcl_Obj 変換用アクセサ
    int                            as_int()        const { return i_; }
    double                         as_double()     const { return d_; }
    bool                           as_bool()       const { return b_; }
    const std::string&             as_string()     const { return *str_; }
    const std::vector<uint8_t>&    as_bytes()      const { return *bytes_; }
    const std::vector<std::string>& as_string_list() const { return *list_; }

private:
    ValueType               type_;
    union
    {
        int     i_;
        double  d_;
        bool    b_;
    };
    std::string*              str_;
    std::vector<uint8_t>*     bytes_;
    std::vector<std::string>* list_;

    void cleanup();
    void copy_from(const ArgValue& other);
};

/** cpp_tkが送出する唯一の例外型。Tcl呼び出し失敗・未初期化オブジェクトへのアクセス等、理由はwhat()で判別する。 */
class Error : public std::runtime_error
{
public:
    using std::runtime_error::runtime_error;
};

/**
 * InterpreterClient::call()/checked_interp()が、呼び出し側にbool* successを渡されなかった場合の挙動を
 * カテゴリ別に指定するビットフラグ。DEFAULT(0)は「対応するビットが立っていない全カテゴリで
 * Errorを送出する」既定値(あえてSTRICTという名前にしていないのは、Windows環境で<windows.h>を
 * 先にincludeするとSTRICTマクロと衝突してコンパイルできなくなるため)。特定カテゴリのビットを
 * 立てると、そのカテゴリだけ「ログのみで空文字列/false/no-opとして継続する(例外を投げない)」
 * に切り替わる。どのカテゴリ・ポリシーでもstderrへのログ出力自体は必ず行われる(診断性は維持する)。
 * 複数カテゴリを組み合わせる場合はoperator|で連結する(例: LENIENT_CALL | LENIENT_THREAD)。
 */
enum class ErrorPolicy : unsigned
{
    DEFAULT        = 0,        // 既定。全カテゴリでErrorを送出する。
    LENIENT_CALL   = 1u << 0,  // Tcl呼び出し失敗・未初期化オブジェクトへのアクセス
    LENIENT_THREAD = 1u << 1,  // Interpreterの所有スレッドと異なるスレッドからのアクセス
};

constexpr ErrorPolicy operator|(ErrorPolicy a, ErrorPolicy b)
{
    return static_cast<ErrorPolicy>(static_cast<unsigned>(a) | static_cast<unsigned>(b));
}

constexpr ErrorPolicy operator&(ErrorPolicy a, ErrorPolicy b)
{
    return static_cast<ErrorPolicy>(static_cast<unsigned>(a) & static_cast<unsigned>(b));
}

/** policyにcategoryのビットが(いずれか一つでも)含まれているかを返す。 */
constexpr bool has_error_policy(ErrorPolicy policy, ErrorPolicy category)
{
    return static_cast<unsigned>(policy & category) != 0;
}

/** 現在のErrorPolicyを設定する(既定はDEFAULT、全カテゴリでErrorを送出する)。 */
void set_error_policy(ErrorPolicy policy);

/** 現在のErrorPolicyを返す。 */
ErrorPolicy error_policy();

/**
 * bind()/command()/trace()/validate()等、Tcl側から起動されるコールバックの中で捕捉された例外の
 * 処理方法を差し替える(既定はstderrへの出力、Python Tk.report_callback_exception相当)。
 * コールバックはTclのCコールスタックから直接呼ばれるため、C++例外をそのまま外へ伝播させると
 * 未定義動作になる。そのためコールバック内の例外は必ずここで捕捉され、Tcl側には伝播しない
 * (ハンドラ自身が例外を投げた場合もそこで握りつぶされ、Tcl側には伝播しない)。
 */
void set_callback_exception_handler(std::function<void(const std::exception&)> handler);

/**
 * Interpreterと1:1で結び付くオブジェクト(Widget/PhotoImage/font::Font/ttk::Style/Var)の共通基底。
 * 派生クラスは「自分のInterpreterポインタがどこに格納されているか」をinterp()で教えるだけで、
 * nullptrガード付きの生Tcl呼び出しcall()が使えるようになる
 * (Python本家におけるself.tk.call(...)のtk部分の置き換えに相当する)。
 */
class InterpreterClient
{

public:

    /**
     * 未ラップのTclサブコマンド/オプションを直接呼び出すためのエスケープハッチ(Python self.tk.call(...)相当)。
     * 名前をinvokeではなくcallにしているのは、本家Python tkinterが同じ役割をself.tk.call(...)と
     * 呼んでいることに合わせるため(invokeはButton等の実在する「クリックを疑似的に発火させる」
     * 公開APIの名前として空けておく)。
     */
    std::string call(const std::vector<ArgValue>& words, bool* success = nullptr) const;

    /**
     * jobを、このオブジェクトが紐づくInterpreter(Tcl_Interp)の所有スレッド上で安全に実行させる。
     * call()等と異なりpost()自体はどのスレッドから呼び出しても安全(Tcl_ThreadQueueEventで対象
     * スレッドのTclイベントループへjobを注入する、Python root.after(0, callback)+queue.Queue相当)。
     * post()の呼び出し自体はjobの完了を待たずに戻る(非同期)。
     */
    void post(std::function<void()> job) const;

protected:

    virtual ~InterpreterClient() = default;

    /** 自分が紐づくInterpreterを返す(未初期化ならnullptr)。派生クラスは自身の格納場所を返すだけでよい。 */
    virtual Interpreter* interp() const = 0;

    /** 診断メッセージに使うクラス名。派生クラスは必要に応じてoverrideする。 */
    virtual const char* type_name() const { return "object"; }

    /**
     * interp()を呼び、nullptrなら「<operation>() called on an uninitialized <type_name()>」という
     * エラーメッセージを出力する。call()以外の操作(Var::get_var/set_var/trace_var等)でも
     * 同じガードを再利用するためのヘルパー。
     * successが非nullptrならその呼び出し元でok判定する前提なので例外は投げない(*successにfalseを設定する)。
     * successがnullptrならerror_policy()に従う(対応するカテゴリがDEFAULTならError送出、
     * LENIENT_*ならログのみで継続する)。
     */
    Interpreter* checked_interp(const char* operation, bool* success = nullptr) const;

};

class Object
{

public:
    std::string id;

    Object();

    Object(const Object&) = default;

    Object(Object&&) = default;

    Object& operator=(const Object& other) { id = other.id; return *this; }

    Object& operator=(Object&& other) noexcept { id = std::move(other.id); return *this; }

private:

    static std::string next();

};

class Var : public Object, public InterpreterClient
{

public:
    Var();

    explicit Var(const Widget& parent, const std::string& type);

    Var(const Var&) = default;

    Var(Var&&) = default;

    Var& operator=(const Var&) = default;

    Var& operator=(Var&&) = default;

    virtual ~Var() = default;

    const std::string& name() const;

    std::string get_var() const;

    void set_var(const std::string& val);

protected:

    Interpreter* interp() const override { return interp_; }

    const char* type_name() const override { return "Var"; }

    Interpreter* interp_;

    std::string  name_;

    // 派生クラスはinterp_->set_var/get_var/trace_varを直接呼ばず、これらのラッパー経由で
    // 呼び出す(未初期化/既に破棄されたinterp_に対するガードはInterpreterClient::checked_interp()に集約)。
    void trace_var(std::function<void(const std::string&)> callback);

    void trace_var(std::function<void(const int&)> callback);

    void trace_var(std::function<void(const double&)> callback);

};

class StringVar : public Var
{

public:

    StringVar() = default;

    StringVar(const Widget& parent);

    void set(const std::string &value);

    std::string get() const;

    void trace(std::function<void(const std::string&)> callback);

};

class BooleanVar : public Var
{
public:

    BooleanVar() = default;

    explicit BooleanVar(const Widget& parent);

    void set(bool value);

    bool get() const; 

    void trace(std::function<void(const bool&)> callback); 

};

class IntVar : public Var
{

public:

    IntVar() = default;

    IntVar(const Widget& parent);

    void set(const int& value);

    int get() const;

    void trace(std::function<void(const int&)> callback);

};

class DoubleVar : public Var
{

public:

    DoubleVar() = default;

    DoubleVar(const Widget& parent);

    void set(const double& value);

    double get() const;

    void trace(std::function<void(const double&)> callback);

};

class PhotoImage : public Object, public InterpreterClient
{
public:
    PhotoImage();

    explicit PhotoImage(const Widget& parent, const std::map<std::string, ArgValue>& options = {});

    const std::string& name() const;

    /**
     * 画像を明示的に破棄する(Tcl "image delete"相当)。cpp_tkのPhotoImageはWidget::destroy()と
     * 同様にRAIIでは破棄せず明示呼び出しに委ねているため、長時間稼働するアプリで多数の画像を
     * 生成・破棄する場合はこれを呼ばないとリークする。
     */
    void destroy();

    /** 画素データを書き込む(Python PhotoImage.put(data, to=...)相当)。dataは"{r g b} {r g b} ..."形式のTclリスト
     *  文字列(行ごとに波括弧で囲んだ画素値の並び)をそのまま渡す(整形はユーザー側で行う簡略版)。 */
    PhotoImage& put(const std::string& data, const std::map<std::string, ArgValue>& options = {});

    /** 指定座標の画素値を"r g b"形式の文字列で返す(Python PhotoImage.get(x, y)相当)。 */
    std::string get(int x, int y) const;

    /** 画像内容を消去し透明にする(Python PhotoImage.blank()相当)。 */
    PhotoImage& blank();

    /** sourceの内容を(options指定範囲で)自分自身へ取り込む(Tclの"imageName copy source"に忠実な版)。 */
    PhotoImage& copy_from(const PhotoImage& source, const std::map<std::string, ArgValue>& options = {});

    /** 自分自身の複製を新規PhotoImageとして作成して返す(Python PhotoImage.copy()相当)。 */
    PhotoImage copy() const;

    /** x(, y)倍に拡大した複製を新規PhotoImageとして作成して返す(Python PhotoImage.zoom(x, y)相当)。 */
    PhotoImage zoom(int x, int y = -1) const;

    /** x(, y)分の1に縮小した複製を新規PhotoImageとして作成して返す(Python PhotoImage.subsample(x, y)相当)。 */
    PhotoImage subsample(int x, int y = -1) const;

    /** 画像の幅(px)を返す(Tcl"image width"相当。本家ではPhotoImage/BitmapImage共通のImage基底が持つ)。 */
    int width() const;

    /** 画像の高さ(px)を返す(Tcl"image height"相当)。 */
    int height() const;

protected:
    Interpreter* interp() const override { return interp_; }

    const char* type_name() const override { return "PhotoImage"; }

private:
    Interpreter* interp_;
    std::string  name_;
};

/**
 * X11ビットマップ(1bit深度)画像を表す(Python tkinter.BitmapImage相当)。本家では
 * PhotoImageと共にImageという共通基底を持つが、cpp_tkのPhotoImageは既にObject/
 * InterpreterClientを直接継承する構造のため、共通基底の導入は見送りPhotoImageと
 * 同じ構造で独立実装している。
 */
class BitmapImage : public Object, public InterpreterClient
{
public:
    BitmapImage();

    explicit BitmapImage(const Widget& parent, const std::map<std::string, ArgValue>& options = {});

    const std::string& name() const;

    /** 画像を明示的に破棄する(Tcl "image delete"相当)。PhotoImage::destroy()と同じ理由で明示呼び出しに委ねる。 */
    void destroy();

    /** 画像の幅(px)を返す(Tcl"image width"相当。本家ではPhotoImage/BitmapImage共通のImage基底が持つ)。 */
    int width() const;

    /** 画像の高さ(px)を返す(Tcl"image height"相当)。 */
    int height() const;

protected:
    Interpreter* interp() const override { return interp_; }

    const char* type_name() const override { return "BitmapImage"; }

private:
    Interpreter* interp_;
    std::string  name_;
};

/** 現在定義されている全画像名の一覧を返す(Tcl "image names"相当)。 */
std::vector<std::string> image_names();

/** サポートされている画像フォーマット種別一覧を返す(Tcl "image types"相当)。 */
std::vector<std::string> image_types();

class Widget : public Object, public InterpreterClient
{

public:

    Widget();

    /**
     * optionsは生成コマンド自体に埋め込む(例: "ttk::panedwindow .p1 -orient horizontal")。
     * これにより、Tkの一部ウィジェットが持つ「生成時にしか指定できない(生成後にconfigureすると
     * "attempt to change read-only option"エラーになる)オプション」(例: ttk::panedwindowの-orient、
     * frame/toplevelの-class/-container/-visual/-colormap/-screen/-use等)も設定できる。
     * 生成後に再設定可能な通常のオプションについては、生成時に埋め込んでも生成後にconfigureするのと
     * 最終的な状態は変わらない。
     */
    Widget(const Widget& parent, const std::string &type, const std::string& name="", const std::map<std::string, ArgValue>& options={});

    // コピーすると「親と同じ型を1引数で渡す」呼び出しが子ウィジェット生成と衝突して
    // 曖昧になり、意図せずコピーコンストラクタが選ばれてしまう事故を防ぐため禁止する
    // (同じ実体を指す複製が欲しい場合はshared_impl()を使う)。
    // 実体(Impl)はshared_ptrで共有されるため、ウィジェットの再配置はムーブで安全に行える。
    Widget(const Widget&) = delete;

    Widget(Widget&&) = default;

    Widget& operator=(const Widget&) = delete;

    Widget& operator=(Widget&&) = default;

    virtual ~Widget() = default;

    const std::string& full_name() const;

    Widget& pack(const std::map<std::string, ArgValue> &options = {});

    Widget& pack_forget();

    Widget& grid(const std::map<std::string, ArgValue> &options = {});

    Widget& grid_forget();

    Widget& place(const std::map<std::string, ArgValue> &option = {});

    Widget& place_forget();

    Widget& config(const std::string& name, const ArgValue& value);

    Widget& config(const std::map<std::string, ArgValue> &option);

    Widget& grid_rowconfigure(int row, const std::map<std::string, ArgValue>& options);

    Widget& grid_columnconfigure(int column, const std::map<std::string, ArgValue>& options);

    std::string cget(const std::string& name) const;

    Widget& bind(const std::string& event, std::function<void(const Event&)> callback);

    /** event に対して bind() で登録した処理を解除する(Python Misc.unbind()相当の簡略版)。 */
    Widget& unbind(const std::string& event);

    /** アプリケーション内の全ウィジェットに対するグローバルなバインドを設定する(Python Misc.bind_all()相当)。 */
    Widget& bind_all(const std::string& event, std::function<void(const Event&)> callback);

    /** bind_all()で登録した処理を解除する(Python Misc.unbind_all()相当の簡略版)。 */
    Widget& unbind_all(const std::string& event);

    /** 指定クラス名(winfo_class()が返す名前)の全ウィジェットに対するバインドを設定する(Python Misc.bind_class()相当)。 */
    Widget& bind_class(const std::string& class_name, const std::string& event, std::function<void(const Event&)> callback);

    std::string after(const int& ms, std::function<void()> callback);

    void after_idle(std::function<void()> callback);

    void after_cancel(const std::string& id);

    void destroy();

    /** 保留中のジオメトリ計算・イベント処理を即時反映させる(Python Misc.update()相当)。 */
    void update();

    /** 保留中の再描画・ジオメトリ計算のみを即時反映させる(Python Misc.update_idletasks()相当)。 */
    void update_idletasks();

    /** 合成イベントを発火する(Python Misc.event_generate()相当)。 */
    void event_generate(const std::string& event, const std::map<std::string, ArgValue>& options = {});

    int winfo_width() const;

    int winfo_height() const;

    /** レイアウト計算前の要求サイズ(Python Misc.winfo_reqwidth()相当)。 */
    int winfo_reqwidth() const;

    /** レイアウト計算前の要求サイズ(Python Misc.winfo_reqheight()相当)。 */
    int winfo_reqheight() const;

    int winfo_x() const;

    int winfo_y() const;

    int winfo_rootx() const;

    int winfo_rooty() const;

    bool winfo_exists() const;

    std::string winfo_class() const;

    std::string winfo_toplevel() const;

    std::vector<std::string> winfo_children() const;

    /** full_name文字列からWidgetハンドルを再構築する(Python Misc.nametowidget()相当の簡略版。
     *  具体的な派生型(Button等)ではなくWidgetとして返るため、config/bind/winfo_*等の共通操作のみ可能)。 */
    Widget nametowidget(const std::string& name) const;

    /** このウィジェットをgridで管理している子ウィジェットのフルネーム一覧(Python Misc.grid_slaves()相当)。 */
    std::vector<std::string> grid_slaves() const;

    /** このウィジェットをpackで管理している子ウィジェットのフルネーム一覧(Python Misc.pack_slaves()相当)。 */
    std::vector<std::string> pack_slaves() const;

    /** このウィジェットをplaceで管理している子ウィジェットのフルネーム一覧(Python Misc.place_slaves()相当)。 */
    std::vector<std::string> place_slaves() const;

    /** 現在のgrid配置オプションを返す(Python Misc.grid_info()相当)。gridで管理されていなければ空。 */
    std::map<std::string, std::string> grid_info() const;

    /** 現在のpack配置オプションを返す(Python Misc.pack_info()相当)。packで管理されていなければ空。 */
    std::map<std::string, std::string> pack_info() const;

    /** 現在のplace配置オプションを返す(Python Misc.place_info()相当)。placeで管理されていなければ空。 */
    std::map<std::string, std::string> place_info() const;

    int winfo_screenwidth() const;

    int winfo_screenheight() const;

    int winfo_pointerx() const;

    int winfo_pointery() const;

    /** このウィジェットを配置しているジオメトリマネージャ名("pack"/"grid"/"place"等)を返す。 */
    std::string winfo_manager() const;

    /** 画面上に実際にマッピングされているか(Python Misc.winfo_ismapped()相当)。 */
    bool winfo_ismapped() const;

    /** キーボードフォーカスをこのウィジェットに設定する(Python Misc.focus_set()相当)。 */
    Widget& focus_set();

    /** grabを無視してでも強制的にフォーカスを設定する(Python Misc.focus_force()相当)。 */
    Widget& focus_force();

    /** 現在フォーカスを持つウィジェットのフルネームを返す(未フォーカス時は空文字列、Python Misc.focus_get()相当)。 */
    std::string focus_get() const;

    /** クリップボードの内容を消去する(Python Misc.clipboard_clear()相当)。 */
    void clipboard_clear();

    /** クリップボードに文字列を追加する(Python Misc.clipboard_append()相当)。 */
    void clipboard_append(const std::string& text);

    /** クリップボードの内容を取得する(Python Misc.clipboard_get()相当)。 */
    std::string clipboard_get() const;

    /**
     * このウィジェットが破棄されるまで呼び出しをブロックする(Python Misc.wait_window()相当)。
     * mainloop()と同様にTclのイベントループへ再入するため、ブロック中も他のイベントは処理される。
     */
    void wait_window() const;

    /** varの値が書き込まれるまで呼び出しをブロックする(Python Misc.wait_variable()相当)。 */
    void wait_variable(const Var& var) const;

    /**
     * このウィジェットが画面上に実際に表示される(viewableになる)まで呼び出しをブロックする
     * (Python Misc.wait_visibility()相当)。モーダルダイアログの定石(wait_visibility()の後に
     * grab_set()する)で使う。
     */
    void wait_visibility() const;

    /** 兄弟ウィジェットの重なり順で最前面に上げる(Python Misc.lift()相当)。 */
    Widget& lift();

    /** 兄弟ウィジェットの重なり順で最背面に下げる(Python Misc.lower()相当)。 */
    Widget& lower();

    /** 実行環境のウィンドウシステムを返す("x11"/"win32"/"aqua"のいずれか、Tcl"tk windowingsystem"相当)。 */
    std::string windowingsystem() const;

    /** システムベルを鳴らす(Python Misc.bell()相当)。 */
    void bell();

    /** 1ポイントあたりのピクセル数(DPIスケール)を返す(Tcl"tk scaling"相当)。 */
    double scaling() const;

    /** 1ポイントあたりのピクセル数(DPIスケール)を設定する。 */
    Widget& scaling(double factor);

    /**
     * イベント配送時にどのバインドタグをどの順序で辿るかを返す(Python Misc.bindtags()の
     * 引数無し版相当)。既定は[このウィジェット, そのクラス名, そのトップレベル, "all"]の4つ。
     */
    std::vector<std::string> bindtags() const;

    /** バインドタグの並びを設定する(Python Misc.bindtags(tagList)相当)。 */
    Widget& bindtags(const std::vector<std::string>& tags);

    /** 現在グラブ中のウィジェットのフルネームを返す(無ければ空文字列、Python Misc.grab_current()相当)。 */
    std::string grab_current() const;

    /** グラブの状態("none"/"local"/"global")を返す(Python Misc.grab_status()相当)。 */
    std::string grab_status() const;

    /** ウィンドウシステム固有のID(16進文字列)を返す(Python Misc.winfo_id()相当)。 */
    std::string winfo_id() const;

    /** ウィジェット名(フルパスの末尾要素)を返す(Python Misc.winfo_name()相当)。 */
    std::string winfo_name() const;

    /** 親ウィジェットのフルネームを返す(トップレベルの場合は空文字列、Python Misc.winfo_parent()相当)。 */
    std::string winfo_parent() const;

    /** 色深度(bit)を返す(Python Misc.winfo_depth()相当)。 */
    int winfo_depth() const;

    /** "widthxheight+x+y"形式の現在のジオメトリ文字列を返す(Python Misc.winfo_geometry()相当)。 */
    std::string winfo_geometry() const;

    /** ルート座標(root_x, root_y)に存在するウィジェットのフルネームを返す(無ければ空文字列、Python Misc.winfo_containing()相当)。 */
    std::string winfo_containing(int root_x, int root_y) const;

    /** Tkオプションデータベースにパターン→値を登録する(Python Misc.option_add()相当)。 */
    void option_add(const std::string& pattern, const std::string& value, const std::string& priority = "");

    /** オプションデータベースから値を取得する(Python Misc.option_get()相当)。該当が無ければ空文字列。 */
    std::string option_get(const std::string& name, const std::string& class_name) const;

    /**
     * フォーカス移動順(Tab順)で次のウィジェットを返す(フォーカス自体は移動しない、Python Misc.tk_focusNext()相当)。
     * nametowidget()と同じ簡略版のため、具体的な派生型ではなくWidgetとして返る。
     */
    Widget tk_focusNext() const;

    /** フォーカス移動順(Tab順)で前のウィジェットを返す(Python Misc.tk_focusPrev()相当)。 */
    Widget tk_focusPrev() const;

protected:

    struct Impl
    {
        Interpreter* interp = nullptr;
        std::string  full_name;
        int          after_id = 0;
    };

public:

    /**
     * 実体(Tclウィジェット)を共有するためのハンドルを返す。Widgetはこの実体への
     * shared_ptrハンドルであり、ここから`SomeType(handle)`のように再構築した複製は、
     * 元のオブジェクトが後でmove/破棄されても常に有効であり続ける。
     * 内部で自分自身や自分の子ウィジェットを参照するコールバック(Tk/Toplevelの
     * WM_DELETE_WINDOWハンドラ、Memo/ScrollableFrameのscrollbar連携等)は、[this]や
     * 生ポインタを直接キャプチャする代わりにこのハンドルをキャプチャし、発火時に
     * `SomeType(handle)`のように再構築してから使うことで、move後のアドレス変化や
     * 破棄後のダングリングポインタによるセグメンテーション違反を避けられる。
     * (本家tkinterのウィジェットが実質的に参照カウント付きのオブジェクトであることに近い。)
     */
    std::shared_ptr<Impl> handle() const { return impl_; }

    /** handle()から実体を共有する複製を作るためのコンストラクタ。 */
    explicit Widget(std::shared_ptr<Impl> impl) : impl_(std::move(impl)) {}

protected:

    std::shared_ptr<Impl> impl_ = std::make_shared<Impl>();

    Interpreter* interp() const override { return impl_->interp; }

    const char* type_name() const override { return "Widget"; }

    // register_*_callbackはimpl_->interp->register_*_callbackを直接呼ばず、これらの
    // ラッパー経由で呼び出す(impl_->interpが未初期化/既に破棄されている場合のガードは
    // InterpreterClient::checked_interp()に集約されている)。
    void register_void_callback(const std::string& name, std::function<void()> callback) const;

    void register_string_callback(const std::string& name, std::function<void(const std::string&)> callback) const;

    void register_double_callback(const std::string& name, std::function<void(const double&)> callback) const;

    void register_event_callback(const std::string& name, std::function<void(const Event&)> callback) const;

    void register_bool_callback(const std::string& name, std::function<bool(const std::string&)> callback) const;
};

class Tk : public Widget
{

public:

    using Widget::Widget; // share<Tk>()用にWidget(shared_ptr<Impl>)を継承する

    explicit Tk();

    Tk& title(const std::string& title);

    Tk& geometry(const std::string &size);

    std::string geometry() const;

    Tk& protocol(const std::string& name, std::function<void()> handler);

    Tk& resizable(bool width, bool height);

    /** タイトルバー・枠を持たないウィンドウにする(Python Tk.overrideredirect()相当)。 */
    Tk& overrideredirect(bool value);

    /** 現在overrideredirect状態かを返す。 */
    bool overrideredirect() const;

    Tk& minsize(int width, int height);

    Tk& maxsize(int width, int height);

    Tk& iconify(); 
    
    Tk& deiconify(); 
    
    Tk& withdraw(); 
    
    Tk& state(const std::string& new_state); 
    
    std::string state() const;

    Tk& attributes(const std::string& name, const std::string& value);

    std::string attributes(const std::string& name) const;

    Tk& lift();
    
    Tk& lower();

    Tk& grab_set();

    Tk& grab_release();

    Tk& iconphoto(const std::string& image_name);

    Tk& iconbitmap(const std::string& bitmap_path);

    /** 最小化時に表示されるアイコンラベルを設定する(Python Tk.wm_iconname()相当)。 */
    Tk& iconname(const std::string& name);

    /** 現在のアイコンラベルを返す。 */
    std::string iconname() const;

    void mainloop();

    void quit();

};

class Frame : public Widget
{

public:

    Frame() = default;

    explicit Frame(const Widget& parent, const std::map<std::string, ArgValue>& options = {});

    Frame& width(const int &width);

    Frame& height(const int &height);

    Frame& grid_propagate(const bool& value);

};

class Toplevel : public Widget
{

public:

    using Widget::Widget; // share<Toplevel>()用にWidget(shared_ptr<Impl>)を継承する

    Toplevel() = default;

    explicit Toplevel(const Widget& parent, const std::map<std::string, ArgValue>& options = {});

    Toplevel& title(const std::string &title_text);

    Toplevel& geometry(const std::string &size);

    std::string geometry() const;

    Toplevel& protocol(const std::string& name, std::function<void()> handler);

    Toplevel& resizable(bool width, bool height);

    /** タイトルバー・枠を持たないウィンドウにする(Python Toplevel.overrideredirect()相当)。 */
    Toplevel& overrideredirect(bool value);

    /** 現在overrideredirect状態かを返す。 */
    bool overrideredirect() const;

    Toplevel& minsize(int width, int height);

    Toplevel& maxsize(int width, int height);

    Toplevel& iconify();

    Toplevel& deiconify();

    Toplevel& withdraw();

    Toplevel& attributes(const std::string& name, const std::string& value);
    
    std::string attributes(const std::string& name) const;

    Toplevel& state(const std::string& new_state);

    std::string state() const;

    Toplevel& lift();
    
    Toplevel& lower();

    Toplevel& grab_set();

    Toplevel& grab_release();

    Toplevel& iconphoto(const std::string& image_name);

    Toplevel& iconbitmap(const std::string& bitmap_path);

    /** 最小化時に表示されるアイコンラベルを設定する(Python Toplevel.wm_iconname()相当)。 */
    Toplevel& iconname(const std::string& name);

    /** 現在のアイコンラベルを返す。 */
    std::string iconname() const;

    /** このウィンドウをmasterに対して一時的な(モーダルダイアログ等の)ウィンドウとして関連付ける。 */
    Toplevel& transient(const Widget& master);

};

class Button : public Widget
{

public:
    Button() = default;

    explicit Button(const Widget& parent, const std::map<std::string, ArgValue>& options = {});

    Button& width(const int& width);

    Button& height(const int& height);

    Button& text(const std::string& text);

    Button& command(std::function<void()> callback);

    /** commandを疑似的に発火させる(Python Button.invoke()相当)。 */
    std::string invoke();

};

class Canvas : public Widget
{

public:

    using Widget::Widget; // share<Canvas>()用にWidget(shared_ptr<Impl>)を継承する

    Canvas() = default;

    explicit Canvas(const Widget& parent, const std::map<std::string, ArgValue>& options = {});

    Canvas& itemconfig(const std::string& id_or_tag, const std::map<std::string, ArgValue>& options);

    /** itemconfigの読み取り版。 */
    std::string itemcget(const std::string& id_or_tag, const std::string& option) const;

    /** 指定タグ/IDの外接矩形(x1 y1 x2 y2)を取得する。該当アイテムが無ければ空を返す。 */
    std::vector<int> bbox(const std::string& id_or_tag) const;

    /** 図形タグ/ID単位のイベントバインド。 */
    Canvas& tag_bind(const std::string& id_or_tag, const std::string& event, std::function<void(const Event&)> callback);

    /** tag_bindで登録したバインドを解除する。 */
    Canvas& tag_unbind(const std::string& id_or_tag, const std::string& event);

    std::string create_line(const std::vector<std::array<double, 2>>& points, const std::map<std::string, ArgValue>& options = {});

    std::string create_line(const int& x1, const int& y1, const int& x2, const int& y2, const std::map<std::string, ArgValue>& options = {});

    std::string create_oval(const int& x1, const int& y1, const int& x2, const int& y2, const std::map<std::string, ArgValue>& options = {});

    std::string create_rectangle(const int& x1, const int& y1, const int& x2, const int& y2, const std::map<std::string, ArgValue>& options = {});

    std::string create_text(const int& x, const int& y, const std::map<std::string, ArgValue>& options = {});

    std::string create_polygon(const std::vector<int>& coords, const std::map<std::string, ArgValue>& options = {}); 
    
    std::string create_arc(int x1, int y1, int x2, int y2, const std::map<std::string, ArgValue>& options = {}); 
    
    std::string create_image(int x, int y, const std::map<std::string, ArgValue>& options = {});

    std::string create_bitmap(int x, int y, const std::map<std::string, ArgValue>& options = {});

    std::string create_window(int x, int y, const Widget& widget, const std::map<std::string, ArgValue>& options = {});

    std::vector<std::string> find_overlapping(int x1, int y1, int x2, int y2) const;

    std::vector<std::string> find_closest(int x, int y) const;

    /** 全アイテムのID一覧を返す(Python Canvas.find_all()相当)。 */
    std::vector<std::string> find_all() const;

    /** 指定タグ/IDを持つアイテムのID一覧を返す(Python Canvas.find_withtag()相当)。 */
    std::vector<std::string> find_withtag(const std::string& tag_or_id) const;

    /** 指定アイテム/タグを兄弟の重なり順で最前面に上げる(above_thisを指定するとその直後に配置)。 */
    Canvas& tag_raise(const std::string& id_or_tag, const std::string& above_this = "");

    /** 指定アイテム/タグを兄弟の重なり順で最背面に下げる(below_thisを指定するとその直前に配置)。 */
    Canvas& tag_lower(const std::string& id_or_tag, const std::string& below_this = "");

    /** スクリーン座標をキャンバス座標(スクロール分を考慮した論理座標)に変換する(Python Canvas.canvasx()相当)。 */
    double canvasx(int screen_x) const;

    /** スクリーン座標をキャンバス座標(スクロール分を考慮した論理座標)に変換する(Python Canvas.canvasy()相当)。 */
    double canvasy(int screen_y) const;

    Canvas& addtag(const std::string& tag, const std::string& where, const std::string& target);
    
    Canvas& dtag(const std::string& tag, const std::string& target); 
    
    std::vector<std::string> gettags(const std::string& id) const;

    Canvas& move(const std::string& id_or_tag, const int& x, const int& y);

    Canvas& moveto(const std::string& id_or_tag, const int& x, const int& y);

    Canvas& xview(const std::string& args);

    Canvas& yview(const std::string& args);

    Canvas& xscrollcommand(std::function<void(std::string)> callback);

    Canvas& yscrollcommand(std::function<void(std::string)> callback);

    Canvas& scale(const std::string& id_or_tag, const int& x, const int& y, const double& xscale, const double& yscale);

    Canvas& coords(const std::string& id_or_tag, const std::vector<int>& coords);

    Canvas& erase(const std::string& id_or_tag);

    Canvas& width(const int &width);

    Canvas& height(const int &height);

    /**
     * 描画内容をPostScript形式で書き出す(Python Canvas.postscript()相当)。"file"オプションを指定しなければ
     * 生成したPostScriptデータを文字列で返す(指定した場合はファイルへ書き出し、戻り値は空文字列)。
     */
    std::string postscript(const std::map<std::string, ArgValue>& options = {});

};

class Checkbutton : public Widget
{

public:

    Checkbutton() = default;

    explicit Checkbutton(const Widget& parent, const std::map<std::string, ArgValue>& options = {});

    Checkbutton& text(const std::string& text);

    /**
     * varはBooleanVar/IntVar/StringVarのいずれも本家同様に利用できる(onvalue/offvalueで対応する値を指定する)。
     * name()を読むだけで保持はしないため、生存期間はこの呼び出し中だけ有効であれば良い。右辺値の一時変数を
     * 弾くためあえて非constの参照にしている。
     */
    Checkbutton& variable(Var& var);

    Checkbutton& command(std::function<void()> callback);

    /** commandを疑似的に発火させる(Python Checkbutton.invoke()相当)。 */
    std::string invoke();

};

class Entry : public Widget
{

public:

    Entry() = default;

    explicit Entry(const Widget& parent, const std::map<std::string, ArgValue>& options = {});

    /**
     * name()を読むだけで保持はしない(Tkの-textvariableはウィジェットの表示テキストとTcl変数を
     * 自動で双方向同期するため、get()/set()相当の操作は常にウィジェット自身を経由すればよく、
     * Var側を後から参照する必要が無い)。右辺値の一時変数を弾くためあえて非constの参照にしている。
     */
    Entry& textvariable(StringVar& var);

    Entry& state(const std::string& state);

    Entry& icursor(const std::string& index);

    Entry& insert(const std::string& index, const std::string& text);

    int index(const std::string& index = "insert") const;

    Entry& erase(const std::string& start, const std::string& end = "");

    std::string get() const;

    /** 横スクロールバーとの連携コールバックを設定する(Python Entry(xscrollcommand=...)相当)。 */
    Entry& xscrollcommand(std::function<void(std::string)> callback);

    /** 横スクロールを行う(Scrollbarの"command"にそのまま渡す想定、Python Entry.xview()相当)。 */
    Entry& xview(const std::string& args);

    /** start〜endの範囲を選択状態にする(Python Entry.select_range()相当)。 */
    Entry& select_range(const std::string& start, const std::string& end);

    /** 選択状態を解除する(Python Entry.select_clear()相当)。 */
    Entry& selection_clear();

    /** 選択範囲が存在するかを返す(Python Entry.select_present()相当)。 */
    bool select_present() const;

    /**
     * 入力値のリアルタイム検証(Python Entry(validate=mode, validatecommand=(vcmd,"%P"))相当の簡略版)。
     * 置換コードは"%P"(検証対象になる編集後の全文字列)のみ対応する(%d/%i/%s/%S/%v/%V/%W等は非対応)。
     * modeは"none"/"focus"/"focusin"/"focusout"/"key"/"all"。callbackがfalseを返すと編集は拒否される。
     */
    Entry& validate(const std::string& mode, std::function<bool(const std::string&)> callback);

};

class Label : public Widget
{

public:

    Label() = default;

    explicit Label(const Widget& parent, const std::map<std::string, ArgValue>& options = {});

    Label& text(const std::string &text);
};

/** classic(非ttk)版のLabelFrame(Python tkinter.LabelFrame相当)。ttk::Labelframeとは別に、本家同様classic側にも用意する。 */
class LabelFrame : public Widget
{

public:

    LabelFrame() = default;

    explicit LabelFrame(const Widget& parent, const std::map<std::string, ArgValue>& options = {});

    LabelFrame& width(const int& width);

    LabelFrame& height(const int& height);

    LabelFrame& text(const std::string& text);
};

class Listbox : public Widget
{

public:

    Listbox() = default;

    explicit Listbox(const Widget& parent, const std::map<std::string, ArgValue>& options = {});

    /** indexはEND/ACTIVE等のシンボリック定数、または数値の文字列表現("0"等)で指定する。 */
    Listbox& insert(const std::string& index, const std::string& item);

    /** endを省略するとstart単体を削除する。 */
    Listbox& erase(const std::string& start, const std::string& end = "");

    std::vector<int> curselection() const;

    std::string get(const std::string& index) const;

    Listbox& yscrollcommand(std::function<void(std::string)> callback);

    /** 縦スクロールを行う(Scrollbarの"command"にそのまま渡す想定、Python Listbox.yview()相当)。 */
    Listbox& yview(const std::string& args);

    /** 横スクロールバーとの連携コールバックを設定する(Python Listbox(xscrollcommand=...)相当)。 */
    Listbox& xscrollcommand(std::function<void(std::string)> callback);

    /** 横スクロールを行う(Python Listbox.xview()相当)。 */
    Listbox& xview(const std::string& args);

    Listbox& selectmode(const std::string& mode);

    /** 指定indexが見えるようスクロールする。 */
    Listbox& see(const std::string& index);

    /** y座標に最も近い行のindexを返す。 */
    int nearest(int y) const;

    /** 行数を返す。 */
    int size() const;

    /** 選択範囲を設定する(lastを省略するとfirst単体を選択)。 */
    Listbox& select_set(const std::string& first, const std::string& last = "");

    /** 選択範囲を解除する(lastを省略するとfirst単体を解除)。 */
    Listbox& select_clear(const std::string& first, const std::string& last = "");

    /** indexが選択されているかを返す。 */
    bool select_includes(const std::string& index) const;

    /** indexをアクティブ要素にする。 */
    Listbox& activate(const std::string& index);

};

class Menu : public Widget 
{ 
public: 

    Menu() = default;

    explicit Menu(const Widget& parent, const std::map<std::string, ArgValue>& options = {}); 
    
    /**
     * callbackを指定すると、クリック時に呼ばれるコールバックを"-command"として自動登録する
     * (Python Menu.add_command(command=callback)相当)。省略時(既定のnullptr)は本家Python同様
     * commandオプション無しの項目になる。
     */
    Menu& add_command(const std::map<std::string, ArgValue>& options, std::function<void()> callback = nullptr);

    Menu& add_cascade(const std::map<std::string, ArgValue>& options);

    Menu& add_separator();

    /** callbackはON/OFF切り替え時に呼ばれる(Python Menu.add_checkbutton(command=callback)相当)。 */
    Menu& add_checkbutton(const std::map<std::string, ArgValue>& options, std::function<void()> callback = nullptr);

    /** callbackは選択時に呼ばれる(Python Menu.add_radiobutton(command=callback)相当)。 */
    Menu& add_radiobutton(const std::map<std::string, ArgValue>& options, std::function<void()> callback = nullptr);

    /**
     * item_typeは"command"/"cascade"/"checkbutton"/"radiobutton"/"separator"のいずれか(Tclの"menu insert"にそのまま対応)。
     * callbackはcommand/checkbutton/radiobutton型の場合のみ意味を持つ(cascade/separatorに指定するとTclがエラーになる)。
     */
    Menu& insert(const std::string& index, const std::string& item_type, const std::map<std::string, ArgValue>& options = {}, std::function<void()> callback = nullptr);

    /**
     * 指定indexの項目の設定を変更する(Python Menu.entryconfigure()相当)。
     * callbackを指定すると、その項目の"-command"を差し替える。
     */
    Menu& entryconfigure(const std::string& index, const std::map<std::string, ArgValue>& options, std::function<void()> callback = nullptr);

    /** patternに一致する項目の数値indexを返す。一致が無ければ-1(Python Menu.index()相当)。 */
    int index(const std::string& pattern) const;

    Menu& erase(const std::string& index);

    /** 任意位置にコンテキストメニューとしてポップアップ表示する。 */
    Menu& post(int x, int y);

    /** postで表示したメニューを閉じる。 */
    Menu& unpost();

};

class Menubutton : public Widget
{ 
    
public: 

    Menubutton() = default;

    explicit Menubutton(const Widget& parent); 
    
    Menubutton& menu(Menu* menu);

};

/**
 * Python tkinter.OptionMenu(master, variable, value, *values)相当。classic専用(ttkに本家同等のものは無い)。
 * 本家同様、内部でMenubutton+Menuを組み立てて実現する。variableは呼び出し側が生存期間を管理する
 * (Checkbutton::variable等の他のvariable系APIと同様の制約)。
 */
class OptionMenu : public Widget
{

public:

    OptionMenu() = default;

    /** valuesの先頭要素が初期値としてvariableに設定される。 */
    explicit OptionMenu(const Widget& parent, StringVar& variable, const std::vector<std::string>& values);

    /** 選択変更時に呼ばれるコールバック(選択された値を引数で受け取る、Python OptionMenu(command=...)相当)。 */
    OptionMenu& command(std::function<void(const std::string&)> callback);

private:

    Menu menu_;

    std::shared_ptr<std::function<void(const std::string&)>> command_;

};

class Message : public Widget
{ 
    
public: 

    Message() = default;

    explicit Message(const Widget& parent);
    
    Message& text(const std::string& text); 

};

class PanedWindow : public Widget 
{ 
    
public: 

    PanedWindow() = default;

    explicit PanedWindow(const Widget& parent); 
    
    PanedWindow& orient(const std::string& dir); 
    
    PanedWindow& add(const Widget& child, const std::map<std::string, ArgValue>& options = {}); 
    
    PanedWindow& forget(const Widget& child); 

};

class Radiobutton : public Widget 
{ 
    
public: 

    Radiobutton() = default;

    explicit Radiobutton(const Widget& parent, const std::map<std::string, ArgValue>& options = {}); 
    
    Radiobutton& text(const std::string& text); 
    
    /** varはStringVar/IntVarのいずれも本家同様に利用できる。name()を読むだけで保持はしないため、
     *  生存期間はこの呼び出し中だけ有効であれば良い。右辺値の一時変数を弾くためあえて非constの参照にしている。 */
    Radiobutton& variable(Var& var);
    
    Radiobutton& value(const std::string& val);

    Radiobutton& command(std::function<void()> callback);

    /** commandを疑似的に発火させる(Python Radiobutton.invoke()相当)。 */
    std::string invoke();

};

class Scale : public Widget
{

public:

    Scale() = default;

    explicit Scale(const Widget& parent, const std::map<std::string, ArgValue>& options = {});

    Scale& from(double val);

    Scale& to(double val);

    Scale& orient(const std::string& dir);

    Scale& command(std::function<void(const double&)> callback);

    /** 現在の値を取得する(Python Scale.get()相当)。 */
    double get() const;

    /** 値を設定する(Python Scale.set()相当)。 */
    Scale& set(double value);

};

class Scrollbar : public Widget
{

public:

    using Widget::Widget; // share<Scrollbar>()用にWidget(shared_ptr<Impl>)を継承する

    Scrollbar() = default;

    explicit Scrollbar(const Widget& parent, const std::map<std::string, ArgValue>& options = {});

    Scrollbar& orient(const std::string& dir);

    Scrollbar& command(std::function<void(const std::string&)> callback);

    Scrollbar& set(const std::string& args);

};

class Spinbox : public Widget
{

public:

    Spinbox() = default;

    explicit Spinbox(const Widget& parent, const std::map<std::string, ArgValue>& options = {});

    Spinbox& from(double val);

    Spinbox& to(double val);

    Spinbox& increment(double val);

    /**
     * 現在の文字列値を取得する(Python Spinbox.get()相当)。Tclのspinbox/ttk::spinboxウィジェット
     * コマンドには"set"サブコマンドが存在しない(Entry::set()を削除したのと同じ理由、
     * docs/tasks.md C節参照)ため、値の設定はtextvariable経由、またはerase()+insert()で行う。
     */
    std::string get() const;
    
    /** name()を読むだけで保持はしない。右辺値の一時変数を弾くためあえて非constの参照にしている。 */
    Spinbox& textvariable(StringVar& var);
    
    Spinbox& command(std::function<void()> callback); 

};

class Text : public Widget
{

public:

    using Widget::Widget; // share<Text>()用にWidget(shared_ptr<Impl>)を継承する

    Text() = default;

    explicit Text(const Widget& parent, const std::map<std::string, ArgValue>& options = {});

    Text& insert(const std::string& index, const std::string& text);

    std::string get(const std::string& start, const std::string& end = "end") const ;

    Text& erase(const std::string& start, const std::string& end = "end");

    Text& yscrollcommand(std::function<void(std::string)> callback);

    Text& yview(const std::string& args);

    /** 横スクロールバーとの連携コールバックを設定する(Python Text(xscrollcommand=...)相当)。 */
    Text& xscrollcommand(std::function<void(std::string)> callback);

    /** 横スクロールを行う(Scrollbarの"command"にそのまま渡す想定、Python Text.xview()相当)。 */
    Text& xview(const std::string& args);

    /** indexの位置の外接矩形("x y width height")を返す。indexが不可視なら空(Python Text.bbox()相当)。 */
    std::vector<int> bbox(const std::string& index) const;

    Text& wrap(const std::string& mode);

    Text& tag_add(const std::string& tag, const std::string& start, const std::string& end); 

    Text& tag_remove(const std::string& tag, const std::string& start, const std::string& end); 

    Text& tag_config(const std::string& tag, const std::map<std::string, ArgValue>& options);

    Text& mark_set(const std::string& mark, const std::string& index); 
    
    Text& mark_unset(const std::string& mark);

    std::string search(const std::string& pattern, const std::string& index, const std::map<std::string, ArgValue>& options = {});

    /** 指定位置が見えるようスクロールする。 */
    Text& see(const std::string& index);

    /** indexで指定した位置を正規化した"line.col"形式の文字列に解決する(Python Text.index()相当)。 */
    std::string index(const std::string& index) const;

    /** 定義されている全タグ名(indexを省略時)、またはindexに付与されているタグ名を返す(Python Text.tag_names()相当)。 */
    std::vector<std::string> tag_names(const std::string& index = "") const;

    /** 指定タグが付与されている範囲(開始・終了indexが交互に並ぶ)を返す(Python Text.tag_ranges()相当)。 */
    std::vector<std::string> tag_ranges(const std::string& tag) const;

    /** 指定タグを重なり順で最前面に上げる(Python Text.tag_raise()相当)。 */
    Text& tag_raise(const std::string& tag, const std::string& above_this = "");

    /** 指定タグを重なり順で最背面に下げる(Python Text.tag_lower()相当)。 */
    Text& tag_lower(const std::string& tag, const std::string& below_this = "");

    /** タグの定義自体を削除する(Python Text.tag_delete()相当)。 */
    Text& tag_delete(const std::string& tag);

    /** 直前の編集操作を取り消す(Python Text.edit_undo()相当)。undoスタックが無効/空なら何もしない。 */
    Text& edit_undo();

    /** undoで取り消した操作をやり直す(Python Text.edit_redo()相当)。 */
    Text& edit_redo();

    /** 変更フラグを取得する(Python Text.edit_modified()相当)。 */
    bool edit_modified() const;

    /** 変更フラグを設定する(Python Text.edit_modified(bool)相当)。 */
    Text& edit_modified(bool value);

    /** index1とindex2の前後関係をopで判定する(opは"<"/"<="/"=="/">="/">"/"!="、Python Text.compare()相当)。 */
    bool compare(const std::string& index1, const std::string& op, const std::string& index2) const;

    /** index1からindex2までの区間をoption("chars"/"lines"/"indices"等)単位でカウントする(Python Text.count()相当の簡略版)。 */
    int count(const std::string& index1, const std::string& index2, const std::string& option = "chars") const;

    /** index1(〜index2)の範囲のタグ/テキスト/マーク等を書き出す(Python Text.dump()相当の簡略版、生のTcl戻り値をそのまま返す)。 */
    std::string dump(const std::string& index1, const std::string& index2 = "") const;

    /** indexの位置に画像を埋め込み、生成された画像アイテム名を返す(Python Text.image_create()相当)。 */
    std::string image_create(const std::string& index, const std::map<std::string, ArgValue>& options = {});

    /** indexの位置に他のウィジェットを埋め込む(Python Text.window_create()相当)。 */
    Text& window_create(const std::string& index, const Widget& window, const std::map<std::string, ArgValue>& options = {});

};

// Python本家ではフォントはtkinter.ttkではなくtkinter.font(別モジュール)に属するため、
// cpp_tkでもttkとは独立したnamespace fontを切る。
namespace font
{

class Font : public Object, public InterpreterClient
{
public:

    Font();

    /**
     * name/existsはPython tkinter.font.Font(root=None, font=None, name=None, exists=False, **options)
     * に対応する。nameを指定しなければ内部で一意な名前を生成する。existsがtrueの場合は
     * "font create"を呼ばず、既存の名前付きフォント(TkDefaultFont等)をそのまま参照する。
     */
    explicit Font(const Widget& parent, const std::map<std::string, ArgValue>& option = {},
                  const std::string& name = "", bool exists = false);

    Font& config(const std::map<std::string, ArgValue>& option);

    Font& size(const int& size);

    Font& weight(const std::string& weight);

    Font& family(const std::string& family);

    Font& slant(const std::string& slant);

    Font& underline(const int& underline);

    Font& overstrike(const int& overstrike);

    /** Python tkinter.font.Font.actual(option)相当。"font actual <name> -<option>"を返す。 */
    std::string actual(const std::string& option) const;

    /** Python tkinter.font.Font.metrics(option)相当。ascent/descent/linespace/fixedのいずれかを返す。 */
    std::string metrics(const std::string& option) const;

    /** Python tkinter.font.Font.measure(text)相当。textの描画幅(px)を返す。 */
    int measure(const std::string& text) const;

    /** 現在の設定を丸ごと引き継いだ複製を新規Fontとして作成して返す(Python tkinter.font.Font.copy()相当)。 */
    Font copy() const;

    const std::string& name() const;

protected:

    Interpreter* interp() const override { return interp_; }

    const char* type_name() const override { return "Font"; }

private:

    Interpreter*    interp_;

    std::string     name_;

};

/**
 * 既存の名前付きフォント(TkDefaultFont等)を参照するFontを返す(Python tkinter.font.nametofont相当)。
 * 本家はFont(name=name, exists=True, root=root)を呼ぶだけの薄いラッパーであり、本実装も同様に
 * 公開コンストラクタを呼ぶだけで、Fontの非公開メンバへ特別にアクセスする必要はない。
 * 本家のrootはNone許容(暗黙のデフォルトルート)だが、cpp_tkはそれを持たないため必須引数にする。
 */
Font nametofont(const Widget& parent, const std::string& name);

/** 利用可能なフォントファミリ名一覧を返す(Python tkinter.font.families()相当)。 */
std::vector<std::string> families();

/** 現在定義されている名前付きフォント名一覧を返す(Python tkinter.font.names()相当)。 */
std::vector<std::string> names();

} // font

namespace ttk
{

/**
 * Python tkinter.ttk.Styleに対応する(configure/map/theme_use/theme_names相当のみ)。
 * Style自体は名前付きTclオブジェクトを生成せず、"ttk::style"/"ttk::setTheme"コマンドを
 * そのまま呼ぶだけなので、Object(id自動採番)は継承しない。InterpreterClient(Interpreterへの
 * 結び付きのみを表す)は継承する。
 */
class Style : public InterpreterClient
{
public:

    Style() = default;

    explicit Style(const Widget& parent);

    /** 指定style(例: "TButton"、全体既定は"." )の既定オプションを設定する。 */
    Style& configure(const std::string& style_name, const std::map<std::string, ArgValue>& options);

    /**
     * 状態依存のオプションを設定する。各オプションの値は"state1 value1 state2 value2 ..."を
     * 表す文字列のリスト(例: {"active", "#3c3f41", "disabled", "#555555"})で渡す
     * (Tclの"-option {state1 value1 ...}"構文にそのまま対応する)。
     */
    Style& map(const std::string& style_name, const std::map<std::string, std::vector<std::string>>& options);

    /** configure/mapで設定した値を読み取る(state/defaultの指定には非対応の簡略版)。 */
    std::string lookup(const std::string& style_name, const std::string& option) const;

    /** 利用可能なテーマ名一覧を返す。 */
    std::vector<std::string> theme_names() const;

    /** テーマを切り替える("clam"/"alt"/"default"等、ネイティブでない純Tcl実装のテーマは配色を上書きしやすい)。 */
    Style& theme_use(const std::string& theme_name);

    /** 現在有効なテーマ名を返す。 */
    std::string theme_use() const;

    /** 指定style(またはelement)の現在のレイアウト仕様を返す(Tclのネストしたリスト形式そのまま、Python Style.layout()の引数無し版相当)。 */
    std::string layout(const std::string& style_name) const;

    /**
     * 指定style(またはelement)のレイアウト仕様を設定する(Python Style.layout(style, layoutspec)相当)。
     * layout_specは"{Checkbutton.padding sticky nswe children {...}}"のようなネストしたTclリスト形式の
     * 文字列をそのまま渡す(薄いラッパーのため、ネスト構造の組み立てはユーザー側に委ねる)。
     */
    Style& layout(const std::string& style_name, const std::string& layout_spec);

    /**
     * 新しいスタイル要素を定義する(Python Style.element_create()相当)。argsはelement type("image"/"from"等)
     * 固有の追加引数をそのまま並べる(例: element_create("Custom.button", "image", {image_name, "-border", 2})相当)。
     */
    Style& element_create(const std::string& name, const std::string& type, const std::vector<ArgValue>& args = {});

    /**
     * 新しいテーマを定義する(Python Style.theme_create()相当)。parentは継承元テーマ名(省略可)、
     * settings_scriptは"ttk::style theme create name -settings { script }"の中括弧内に相当する生Tclスクリプト
     * 文字列で、要素定義・configure/map呼び出しをまとめて指定する(高度なテーマカスタマイズ向けの薄いラッパーの
     * ため、スクリプト自体の組み立てはユーザーに委ねる)。
     */
    Style& theme_create(const std::string& name, const std::string& parent = "", const std::string& settings_script = "");

protected:

    Interpreter* interp() const override { return interp_; }

    const char* type_name() const override { return "Style"; }

private:

    Interpreter* interp_ = nullptr;

};

class Button : public Widget
{

public:

    Button() = default;

    explicit Button(const Widget& parent, const std::map<std::string, ArgValue>& options = {});

    Button& width(const int& width);

    Button& height(const int& height);

    Button& text(const std::string& text);

    Button& command(std::function<void()> callback);

    Button& font(const font::Font& font);

    /** commandを疑似的に発火させる(Python Button.invoke()相当)。 */
    std::string invoke();

};

class Checkbutton : public Widget
{

public:

    Checkbutton() = default;

    explicit Checkbutton(const Widget& parent, const std::map<std::string, ArgValue>& options = {});

    Checkbutton& text(const std::string& text);

    /** varはBooleanVar/IntVar/StringVarのいずれも本家同様に利用できる。name()を読むだけで保持はしないため、
     *  生存期間はこの呼び出し中だけ有効であれば良い。右辺値の一時変数を弾くためあえて非constの参照にしている。 */
    Checkbutton& variable(Var& var);

    Checkbutton& command(std::function<void()> callback);

    /** commandを疑似的に発火させる(Python Checkbutton.invoke()相当)。 */
    std::string invoke();

};

class Combobox : public Widget
{

public:

    Combobox() = default;

    explicit Combobox(const Widget& parent, const std::map<std::string, ArgValue>& options = {});

    Combobox& values(const std::vector<std::string>& items);

    /** name()を読むだけで保持はしない。右辺値の一時変数を弾くためあえて非constの参照にしている。 */
    Combobox& textvariable(StringVar& var);

    Combobox& width(const int& width);

    Combobox& height(const int& height);

    Combobox& justify(const std::string& justify);

    Combobox& state(const std::string& state);

    Combobox& font(const font::Font& font);

    /** 本家ttk::Combobox.set()/Tclの"ttk::combobox set"に対応する。 */
    Combobox& set(const std::string& value);

    Combobox& insert(const std::string& index, const std::string& text);

    /** 本家ttk::Combobox.get()/Tclの"ttk::combobox get"に対応する。 */
    std::string get() const;

    Combobox& erase(const std::string& start, const std::string& end);

    int current() const;

    Combobox& current(const int& idx);

};

class Entry : public Widget
{

public:

    Entry() = default;

    explicit Entry(const Widget& parent, const std::map<std::string, ArgValue>& options = {});

    /** name()を読むだけで保持はしない。右辺値の一時変数を弾くためあえて非constの参照にしている。 */
    Entry& textvariable(StringVar& var);

    Entry& state(const std::string& state);

    Entry& icursor(const std::string& index);

    Entry& insert(const std::string& index, const std::string& text);

    int index(const std::string& index = "insert") const;

    Entry& erase(const std::string& start, const std::string& end = "");

    std::string get() const;

    Entry& font(const font::Font& font);

    /** 横スクロールバーとの連携コールバックを設定する(Python Entry(xscrollcommand=...)相当)。 */
    Entry& xscrollcommand(std::function<void(std::string)> callback);

    /** 横スクロールを行う(Scrollbarの"command"にそのまま渡す想定、Python Entry.xview()相当)。 */
    Entry& xview(const std::string& args);

    /** start〜endの範囲を選択状態にする(Python Entry.select_range()相当)。 */
    Entry& select_range(const std::string& start, const std::string& end);

    /** 選択状態を解除する(Python Entry.select_clear()相当)。 */
    Entry& selection_clear();

    /** 選択範囲が存在するかを返す(Python Entry.select_present()相当)。 */
    bool select_present() const;

    /**
     * 入力値のリアルタイム検証(Python Entry(validate=mode, validatecommand=(vcmd,"%P"))相当の簡略版)。
     * 置換コードは"%P"(検証対象になる編集後の全文字列)のみ対応する(%d/%i/%s/%S/%v/%V/%W等は非対応)。
     * modeは"none"/"focus"/"focusin"/"focusout"/"key"/"all"。callbackがfalseを返すと編集は拒否される。
     */
    Entry& validate(const std::string& mode, std::function<bool(const std::string&)> callback);

};

class Frame : public Widget
{

public:

    using Widget::Widget; // share<Frame>()用にWidget(shared_ptr<Impl>)を継承する

    Frame() = default;

    explicit Frame(const Widget& parent, const std::map<std::string, ArgValue>& options = {});

    Frame& width(const int &width);

    Frame& height(const int &height);

};

class Notebook : public Widget 
{

public:
    
    Notebook() = default;

    explicit Notebook(const Widget& parent, const std::map<std::string, ArgValue>& options = {});

    Notebook& add_tab(const Widget& child, const std::string& label);

    /** tab_idは数値indexの文字列表現、タブ内容ウィジェットのフルネーム、または"current"を受け付ける(Python Notebook.select()相当)。 */
    Notebook& select(const std::string& tab_id);

    /** 現在選択されているタブのウィンドウ名を返す(Python Notebook.select()の引数無し版相当)。 */
    std::string select() const;

    /** タブの設定を変更する(Python Notebook.tab(tab_id, **kw)相当)。 */
    Notebook& tab(const std::string& tab_id, const std::map<std::string, ArgValue>& options);

    /** タブの設定値を1つ読み取る(Python Notebook.tab(tab_id, option)相当)。 */
    std::string tab(const std::string& tab_id, const std::string& option) const;

    /** 管理下にある全タブ(ウィンドウのフルネーム)一覧を返す(Python Notebook.tabs()相当)。 */
    std::vector<std::string> tabs() const;

    /** タブを取り除き、ウィンドウの管理も解除する(Python Notebook.forget()相当)。 */
    Notebook& forget(const std::string& tab_id);

    /** タブを一時的に非表示にする(再度add_tabやtab()経由で復帰可能、Python Notebook.hide()相当)。 */
    Notebook& hide(const std::string& tab_id);

    /** タブの数値indexを返す(Python Notebook.index()相当)。 */
    int index(const std::string& tab_id) const;
};

/** ttk版のPanedWindow(Python tkinter.ttk.PanedWindow相当)。 */
class PanedWindow : public Widget
{
public:
    PanedWindow() = default;

    explicit PanedWindow(const Widget& parent, const std::map<std::string, ArgValue>& options = {});

    PanedWindow& orient(const std::string& dir);

    PanedWindow& add(const Widget& child, const std::map<std::string, ArgValue>& options = {});

    PanedWindow& forget(const Widget& child);

    /** 指定indexの区切り(sash)のX/Y座標を取得する(Python PanedWindow.sashpos()の引数無し版相当)。 */
    int sashpos(int index) const;

    /** 指定indexの区切り(sash)の位置を変更する(Python PanedWindow.sashpos(index, newpos)相当)。 */
    PanedWindow& sashpos(int index, int newpos);

    /** 管理下にある全ペイン(ウィンドウのフルネーム)一覧を返す(Python PanedWindow.panes()相当)。 */
    std::vector<std::string> panes() const;
};

class Label : public Widget
{

public:

    using Widget::Widget; // share<Label>()用にWidget(shared_ptr<Impl>)を継承する

    Label() = default;

    explicit Label(const Widget& parent, const std::map<std::string, ArgValue>& options = {});

    Label& text(const std::string &text);

    Label& anchor(const std::string& anchor);

    Label& relief(const std::string& relief);

    Label& font(const font::Font& font);
};

class Labelframe : public Widget
{
public:
    Labelframe() = default;

    explicit Labelframe(const Widget& parent, const std::map<std::string, ArgValue>& options = {});

    Labelframe& text(const std::string& text);
};

/** ttk版のMenubutton(Python tkinter.ttk.Menubutton相当)。menu()には引き続きclassicのMenuを紐づける
 *  (ttkに専用のMenuクラスは存在せず、本家同様classicのMenuを共用する)。 */
class Menubutton : public Widget
{
public:
    Menubutton() = default;

    explicit Menubutton(const Widget& parent, const std::map<std::string, ArgValue>& options = {});

    Menubutton& menu(Menu* menu);
};

class Progressbar : public Widget
{
public:
    Progressbar() = default;

    explicit Progressbar(const Widget& parent, const std::map<std::string, ArgValue>& options = {});

    Progressbar& mode(const std::string& mode);

    Progressbar& value(double v);

    Progressbar& start(int interval = 50);

    Progressbar& stop();

    Progressbar& step(double amount = 1.0);

};

class Radiobutton : public Widget 
{ 
    
public: 

    Radiobutton() = default;

    explicit Radiobutton(const Widget& parent, const std::map<std::string, ArgValue>& options = {}); 
    
    Radiobutton& text(const std::string& text); 
    
    /** varはStringVar/IntVarのいずれも本家同様に利用できる。name()を読むだけで保持はしないため、
     *  生存期間はこの呼び出し中だけ有効であれば良い。右辺値の一時変数を弾くためあえて非constの参照にしている。 */
    Radiobutton& variable(Var& var);
    
    Radiobutton& value(const std::string& val);

    Radiobutton& command(std::function<void()> callback);

    /** commandを疑似的に発火させる(Python Radiobutton.invoke()相当)。 */
    std::string invoke();

};

class Separator : public Widget
{
public:
    Separator() = default;

    explicit Separator(const Widget& parent, const std::map<std::string, ArgValue>& options = {});
};

class Scale : public Widget
{

public:

    using Widget::Widget; // share<Scale>()用にWidget(shared_ptr<Impl>)を継承する

    Scale() = default;

    explicit Scale(const Widget& parent, const std::map<std::string, ArgValue>& options = {});

    Scale& from(double val);

    Scale& to(double val);

    Scale& orient(const std::string& dir);

    Scale& command(std::function<void(const double&)> callback);

    /** 現在の値を取得する(Python Scale.get()相当)。 */
    double get() const;

    /** 値を設定する(Python Scale.set()相当)。 */
    Scale& set(double value);

};

class Scrollbar : public Widget
{

public:

    using Widget::Widget; // share<Scrollbar>()用にWidget(shared_ptr<Impl>)を継承する

    Scrollbar() = default;

    explicit Scrollbar(const Widget& parent, const std::map<std::string, ArgValue>& options = {});

    Scrollbar& orient(const std::string& dir);

    Scrollbar& command(std::function<void(const std::string&)> callback);

    Scrollbar& set(const std::string& args);

};

class Spinbox : public Widget 
{ 
    
public: 

    Spinbox() = default;

    explicit Spinbox(const Widget& parent, const std::map<std::string, ArgValue>& options = {}); 
    
    Spinbox& from(double val); 
    
    Spinbox& to(double val); 
    
    Spinbox& increment(double val);

    /** 現在の文字列値を取得する(Python Spinbox.get()相当)。setが無い理由はclassic版のコメント参照。 */
    std::string get() const;

    /** name()を読むだけで保持はしない。右辺値の一時変数を弾くためあえて非constの参照にしている。 */
    Spinbox& textvariable(StringVar& var);
    
    Spinbox& command(std::function<void()> callback); 

};

class Sizegrip : public Widget
{
public:
    Sizegrip() = default;

    explicit Sizegrip(const Widget& parent, const std::map<std::string, ArgValue>& options = {});
};

class Treeview : public Widget
{
public:
    Treeview() = default;

    explicit Treeview(const Widget& parent, const std::map<std::string, ArgValue>& options = {});

    Treeview& insert(const std::string& parent, const std::string& index, const std::string& iid, const std::map<std::string, ArgValue>& options = {});

    Treeview& erase(const std::string& iid);

    Treeview& item(const std::string& iid, const std::map<std::string, ArgValue>& options = {});
    
    /**
     * callbackを指定すると、列ヘッダクリック時に呼ばれるコールバックを"-command"として自動登録する
     * (Python Treeview.heading(column, command=callback)相当。「ヘッダクリックでソート」の定番パターン用)。
     */
    Treeview& heading(const std::string& column, const std::map<std::string, ArgValue>& options = {}, std::function<void()> callback = nullptr);

    Treeview& column(const std::string& column, const std::map<std::string, ArgValue>& options = {});

    std::vector<std::string> selection() const;

    /** 選択範囲を置き換える。 */
    Treeview& selection_set(const std::vector<std::string>& iids);

    /** 選択範囲に追加する。 */
    Treeview& selection_add(const std::vector<std::string>& iids);

    /** 選択範囲から取り除く。 */
    Treeview& selection_remove(const std::vector<std::string>& iids);

    /** 選択状態を反転させる。 */
    Treeview& selection_toggle(const std::vector<std::string>& iids);

    /** 指定iidの行が存在するかを返す。 */
    bool exists(const std::string& iid) const;

    /** 指定行が見えるようスクロールする。 */
    Treeview& see(const std::string& iid);

    Treeview& xview(const std::string& args);

    Treeview& yview(const std::string& args);

    Treeview& xscrollcommand(std::function<void(std::string)> callback);

    Treeview& yscrollcommand(std::function<void(std::string)> callback);

    Treeview& set(const std::string& iid, const std::string& column, const ArgValue& value);

    std::string set(const std::string& iid, const std::string& column) const;

    Treeview& move(const std::string& iid, const std::string& parent, const std::string& index);

    Treeview& detach(const std::string& iid);

    Treeview& reattach(const std::string& iid, const std::string& parent, const std::string& index);

    std::vector<std::string> get_children(const std::string& iid = "") const;

    std::string parent(const std::string& iid) const;

    int index(const std::string& iid) const;

    Treeview& focus(const std::string& iid);
    
    std::string focus() const;

    Treeview& tag_configure(const std::string& tag, const std::map<std::string, ArgValue>& options);

    Treeview& tag_bind(const std::string& tag, const std::string& event, std::function<void(const Event&)> callback);

    /** 指定タグが付与されている行のiid一覧を返す(Python Treeview.tag_has()相当)。 */
    std::vector<std::string> tag_has(const std::string& tag) const;

    std::string identify_row(int y) const;

    std::string identify_column(int x) const;

    std::vector<int> bbox(const std::string& iid, const std::string& column = "") const;
};

} // ttk

namespace colorchooser
{
    std::string askcolor(const std::map<std::string, ArgValue>& options = {});
} // colorchooser

namespace filedialog
{

/**
 * ファイル選択ダイアログを表示し、選択されたパスを返す(空文字列はキャンセル)。
 * 名前はPython tkinter.filedialog.askopenfilename()に合わせているが、Python本家のaskopenfile()は
 * パスではなくファイルオブジェクトを返す点で意味が異なることに注意。
 */
std::string askopenfile(const std::map<std::string, ArgValue>& options = {});

/** 複数選択(-multiple 1)を指定してファイル選択ダイアログを表示し、選択されたパス一覧を返す(Python askopenfilenames()相当)。 */
std::vector<std::string> askopenfilenames(const std::map<std::string, ArgValue>& options = {});

std::string asksaveasfilename(const std::map<std::string, ArgValue>& options = {});

std::string askdirectory(const std::map<std::string, ArgValue>& options = {});

} // filedialog

namespace messagebox 
{

std::string showinfo(const std::string& title, const std::string& message);

std::string showwarning(const std::string& title, const std::string& message);

std::string showerror(const std::string& title, const std::string& message);

std::string askquestion(const std::string& title, const std::string& message);

bool askyesno(const std::string& title, const std::string& message);

bool askokcancel(const std::string& title, const std::string& message);

bool askretrycancel(const std::string& title, const std::string& message);

/** yes/no/cancelの3択ダイアログ(Python messagebox.askyesnocancel()相当)。キャンセル時はfalseを返す点はaskyesno()と同じ判定に倣う。 */
bool askyesnocancel(const std::string& title, const std::string& message);

} // messagebox

} // cpp_tk

#endif // CPP_TK_HPP
