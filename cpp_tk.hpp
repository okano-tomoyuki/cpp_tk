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
 * InterpreterClient::call()/checked_interp()が、呼び出し側にbool* successを渡されなかった場合の挙動。
 * どちらのポリシーでもstderrへのログ出力自体は必ず行われる(診断性は維持する)。
 */
enum class ErrorPolicy
{
    STRICT,  // 既定。Errorを送出する(開発時向け)。
    LENIENT, // 例外を投げず、ログのみで空文字列/false/no-opとして継続する(Release向け)。
};

/** 現在のErrorPolicyを設定する(既定はSTRICT)。 */
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
     * successがnullptrならerror_policy()に従う(STRICTならError送出、LENIENTならログのみで継続する)。
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

protected:
    Interpreter* interp() const override { return interp_; }

    const char* type_name() const override { return "PhotoImage"; }

private:
    Interpreter* interp_;
    std::string  name_;
};


class Widget : public Object, public InterpreterClient
{

public:

    Widget();

    Widget(const Widget& parent, const std::string &type, const std::string& name="");

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

    int winfo_x() const;

    int winfo_y() const;

    int winfo_rootx() const;

    int winfo_rooty() const;

    bool winfo_exists() const;

    std::string winfo_class() const;

    std::string winfo_toplevel() const;

    std::vector<std::string> winfo_children() const;

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

    /** 兄弟ウィジェットの重なり順で最前面に上げる(Python Misc.lift()相当)。 */
    Widget& lift();

    /** 兄弟ウィジェットの重なり順で最背面に下げる(Python Misc.lower()相当)。 */
    Widget& lower();

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
    
    std::string create_window(int x, int y, const Widget& widget, const std::map<std::string, ArgValue>& options = {}); 
    
    std::vector<std::string> find_overlapping(int x1, int y1, int x2, int y2) const; 
    
    std::vector<std::string> find_closest(int x, int y) const; 
    
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
    
    Menu& add_command(const std::map<std::string, ArgValue>& options);

    Menu& add_cascade(const std::map<std::string, ArgValue>& options);

    Menu& add_separator();

    Menu& add_checkbutton(const std::map<std::string, ArgValue>& options);

    Menu& add_radiobutton(const std::map<std::string, ArgValue>& options);

    /** item_typeは"command"/"cascade"/"checkbutton"/"radiobutton"/"separator"のいずれか(Tclの"menu insert"にそのまま対応)。 */
    Menu& insert(const std::string& index, const std::string& item_type, const std::map<std::string, ArgValue>& options = {});

    /** 指定indexの項目の設定を変更する(Python Menu.entryconfigure()相当)。 */
    Menu& entryconfigure(const std::string& index, const std::map<std::string, ArgValue>& options);

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

};

class Scrollbar : public Widget 
{

public:

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

    Text& wrap(const std::string& mode);

    Text& tag_add(const std::string& tag, const std::string& start, const std::string& end); 

    Text& tag_remove(const std::string& tag, const std::string& start, const std::string& end); 

    Text& tag_config(const std::string& tag, const std::map<std::string, ArgValue>& options);

    Text& mark_set(const std::string& mark, const std::string& index); 
    
    Text& mark_unset(const std::string& mark);

    std::string search(const std::string& pattern, const std::string& index, const std::map<std::string, ArgValue>& options = {});

    /** 指定位置が見えるようスクロールする。 */
    Text& see(const std::string& index);

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
};

class Label : public Widget
{

public:

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

    Scale() = default;

    explicit Scale(const Widget& parent, const std::map<std::string, ArgValue>& options = {});

    Scale& from(double val);

    Scale& to(double val);

    Scale& orient(const std::string& dir);

    Scale& command(std::function<void(const double&)> callback);

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
    
    Treeview& heading(const std::string& column, const std::map<std::string, ArgValue>& options = {});

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
