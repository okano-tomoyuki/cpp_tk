#include <cstring>
#include <string>
#include <functional>
#include <map>
#include <unordered_map>
#include <thread>
#include <vector>
#include <iostream>
#include <array>
#include <cstdint>


#include <tk.h>
#include <tcl.h>

#include "cpp_tk.hpp"

namespace 
{

long safe_stol(const char* s)
{
    if (!s || *s == '\0' || std::strcmp(s, "??") == 0) 
        return 0;

    char* endptr = nullptr;
    int ret = std::strtol(s, &endptr, 10);

    if (endptr == s || *endptr != '\0') 
        return 0;

    return ret;
}

double safe_stod(const char* s)
{
    if (!s || *s == '\0' || std::strcmp(s, "??") == 0) 
        return 0.0;

    char* endptr = nullptr;
    double ret = std::strtod(s, &endptr);

    if (endptr == s || *endptr != '\0') 
        return 0.0;

    return ret;
}

std::string sanitize(const std::string& s)
{
    std::string ret;
    for (char c : s)
    {
        ret += std::isalnum(c) ? c : '_';
    }
    return ret;
}

// Menuの各項目("-command")用の一意なTclプロシージャ名を生成する。項目はindexでしか
// 識別できず、indexは挿入/削除で変動するため、Menuインスタンス単位ではなくプロセス全体で
// 単調増加するカウンタを使って衝突を避ける(以前設定した項目のindexが後から他の項目に
// ずれても、既存の"-command"が指す名前付きプロシージャ自体は変わらないため実害は無い)。
std::string next_menu_entry_callback_name(const std::string& menu_full_name)
{
    static int counter = 0;
    return sanitize(menu_full_name) + "_menu_entry_cb_" + std::to_string(counter++);
}

// ArgValue を対応する Tcl_Obj* に変換する内部ヘルパー
Tcl_Obj* make_obj(Tcl_Interp* interp, const cpp_tk::ArgValue& v)
{
    switch (v.type())
    {
        case cpp_tk::ArgValue::ValueType::STRING:
            return Tcl_NewStringObj(v.as_string().c_str(), (int)v.as_string().size());
        case cpp_tk::ArgValue::ValueType::INT:
            return Tcl_NewIntObj(v.as_int());
        case cpp_tk::ArgValue::ValueType::DOUBLE:
            return Tcl_NewDoubleObj(v.as_double());
        case cpp_tk::ArgValue::ValueType::BOOL:
            return Tcl_NewBooleanObj(v.as_bool() ? 1 : 0);
        case cpp_tk::ArgValue::ValueType::BYTES:
            return Tcl_NewByteArrayObj(v.as_bytes().data(), (int)v.as_bytes().size());
        case cpp_tk::ArgValue::ValueType::LIST:
        {
            Tcl_Obj* list_obj = Tcl_NewListObj(0, nullptr);
            for (const auto& item : v.as_list())
            {
                // 要素自身がLIST/DICTの場合も含め、make_obj()を再帰呼び出しすることで
                // 任意の深さのネストを正しく組み立てる。
                Tcl_ListObjAppendElement(interp, list_obj, make_obj(interp, item));
            }
            return list_obj;
        }
        case cpp_tk::ArgValue::ValueType::DICT:
        {
            Tcl_Obj* dict_obj = Tcl_NewDictObj();
            for (const auto& kv : v.as_dict())
            {
                Tcl_Obj* key_obj = Tcl_NewStringObj(kv.first.c_str(), (int)kv.first.size());
                Tcl_DictObjPut(interp, dict_obj, key_obj, make_obj(interp, kv.second));
            }
            return dict_obj;
        }
        default:
            return Tcl_NewObj();
    }
}

}

namespace cpp_tk
{

std::unordered_map<std::thread::id, Interpreter*> interp_map;

static ErrorPolicy g_error_policy = ErrorPolicy::DEFAULT;

void set_error_policy(ErrorPolicy policy)
{
    g_error_policy = policy;
}

ErrorPolicy error_policy()
{
    return g_error_policy;
}

// checked_interp()/call()共通の失敗時挙動。ログ出力は常に行い、successが渡されていれば
// (呼び出し元でok判定する前提なので)例外は投げない。successがnullptrなら、categoryに対応する
// ビットがerror_policy()に含まれているかで判定する(含まれていなければStrict扱いでErrorを送出)。
static void report_or_throw(const std::string& message, bool* success, ErrorPolicy category)
{
    std::cerr << "cpp_tk Error: " << message << std::endl;
    if (success)
    {
        *success = false;
        return;
    }
    if (!has_error_policy(error_policy(), category))
        throw Error(message);
}

static std::function<void(const std::exception&)> g_callback_exception_handler =
    [](const std::exception& e) {
        std::cerr << "cpp_tk Error: uncaught exception in callback: " << e.what() << std::endl;
    };

void set_callback_exception_handler(std::function<void(const std::exception&)> handler)
{
    g_callback_exception_handler = std::move(handler);
}

// register_*_callback/trace_varのトランポリン(TclのCコールスタックから直接呼ばれる)専用。
// コールバック本体は必ずこれ経由で呼び出し、C++例外がTcl側のCフレームへ伝播しないようにする。
// ハンドラ自体が例外を投げても、ここで握りつぶしてTcl側には一切伝播させない。
static void invoke_guarded(const std::function<void()>& body)
{
    try
    {
        body();
    }
    catch (const std::exception& e)
    {
        try { g_callback_exception_handler(e); } catch (...) {}
    }
    catch (...)
    {
        try { g_callback_exception_handler(std::runtime_error("unknown exception in cpp_tk callback")); } catch (...) {}
    }
}

class Interpreter
{

public:

    Interpreter()
        : interp_(nullptr)
        , owner_thread_(std::this_thread::get_id())
        , owner_tcl_thread_(Tcl_GetCurrentThread())
    {
        interp_ = Tcl_CreateInterp();
        Tcl_Init(interp_);
        Tk_Init(interp_);
    }

    ~Interpreter()
    {
        Tcl_DeleteInterp(interp_);
        interp_ = nullptr;
    }

    // Tcl_Interpは生成したスレッド以外から触ってはいけない(スレッド間で共有するAPIではない)。
    // 呼び出し側(InterpreterClient::checked_interp)がこのスレッドIDと現在のスレッドを比較し、
    // クロススレッドアクセスを検知するために公開する。
    std::thread::id owner_thread() const { return owner_thread_; }

    // Tcl_ThreadQueueEventで対象スレッドのTclイベントループへjobを安全に注入する(post()相当の実処理)。
    // このメソッド自体はどのスレッドから呼んでも安全(Tcl_ThreadQueueEvent/Tcl_ThreadAlertはスレッド
    // セーフなAPIとしてTcl自身が提供している)。
    void post(std::function<void()> job)
    {
        auto* evPtr = reinterpret_cast<PostedJobEvent*>(Tcl_Alloc(sizeof(PostedJobEvent)));
        evPtr->header.proc     = &Interpreter::handle_posted_job;
        evPtr->header.nextPtr  = nullptr;
        evPtr->job             = new std::function<void()>(std::move(job));
        Tcl_ThreadQueueEvent(owner_tcl_thread_, reinterpret_cast<Tcl_Event*>(evPtr), TCL_QUEUE_TAIL);
        Tcl_ThreadAlert(owner_tcl_thread_);
    }

    std::string call(const std::vector<ArgValue>& words, bool* success = nullptr)
    {
        std::vector<Tcl_Obj*> objv;
        objv.reserve(words.size());
        for (const auto& w : words)
            objv.push_back(make_obj(interp_, w));

        for (auto* obj : objv)
            Tcl_IncrRefCount(obj);

        int code = Tcl_EvalObjv(interp_, (int)objv.size(), objv.data(), 0);

        bool ok = (code == TCL_OK);
        if (!ok)
        {
            std::cerr << "Tcl Error: " << Tcl_GetStringResult(interp_) << " (command:";
            for (auto* obj : objv)
                std::cerr << " " << Tcl_GetString(obj);
            std::cerr << ")" << std::endl;
        }

        for (auto* obj : objv)
            Tcl_DecrRefCount(obj);

        if (success)
            *success = ok;
        return Tcl_GetStringResult(interp_);
    }

    // Tclのリスト形式の文字列(要素にスペースを含む場合は{}や""で囲まれる)を、
    // 単純な空白split(既存のwinfo_children等が使っている方式)では壊れてしまうため、
    // Tcl_SplitListで正しく要素分解する。
    std::vector<std::string> split_list(const std::string& list) const
    {
        int argc = 0;
        const char** argv = nullptr;
        std::vector<std::string> result;
        if (Tcl_SplitList(interp_, list.c_str(), &argc, &argv) == TCL_OK)
        {
            result.reserve(argc);
            for (int i = 0; i < argc; ++i)
                result.emplace_back(argv[i]);
            Tcl_Free(reinterpret_cast<char*>(argv));
        }
        return result;
    }

    void set_var(const std::string& name, const std::string& value)
    {
        Tcl_SetVar(interp_, name.c_str(), value.c_str(), TCL_GLOBAL_ONLY);
    }
    
    std::string get_var(const std::string& name)
    {
        const char* val = Tcl_GetVar(interp_, name.c_str(), TCL_GLOBAL_ONLY);
        return val ? val : "";
    }

    void trace_var(const std::string& name, std::function<void(const std::string&)> callback)
    {
        string_callback_map_[name] = callback;
        Tcl_TraceVar(interp_, name.c_str(), TCL_TRACE_WRITES, [](ClientData client_data, Tcl_Interp* interp, const char* name1, const char* name2, int flags) -> char* {
            auto* self = static_cast<Interpreter*>(client_data);
            auto it = self->string_callback_map_.find(name1);
            if (it != self->string_callback_map_.end()) {
                const char* val = Tcl_GetVar(interp, name1, TCL_GLOBAL_ONLY);
                std::string v = val ? val : "";
                invoke_guarded([&]() { it->second(v); });
            }
            return nullptr;
        }, this);
    }

    void trace_var(const std::string& name, std::function<void(const int&)> callback)
    {
        int_callback_map_[name] = callback;
        Tcl_TraceVar(interp_, name.c_str(), TCL_TRACE_WRITES, [](ClientData client_data, Tcl_Interp* interp, const char* name1, const char* name2, int flags) -> char* {
            auto* self = static_cast<Interpreter*>(client_data);
            auto it = self->int_callback_map_.find(name1);
            if (it != self->int_callback_map_.end()) {
                const char* val = Tcl_GetVar(interp, name1, TCL_GLOBAL_ONLY);
                int v = val ? std::stol(val) : 0;
                invoke_guarded([&]() { it->second(v); });
            }
            return nullptr;
        }, this);
    }

    void trace_var(const std::string& name, std::function<void(const double&)> callback)
    {
        double_callback_map_[name] = callback;
        Tcl_TraceVar(interp_, name.c_str(), TCL_TRACE_WRITES, [](ClientData client_data, Tcl_Interp* interp, const char* name1, const char* name2, int flags) -> char* {
            auto* self = static_cast<Interpreter*>(client_data);
            auto it = self->double_callback_map_.find(name1);
            if (it != self->double_callback_map_.end()) {
                const char* val = Tcl_GetVar(interp, name1, TCL_GLOBAL_ONLY);
                double v = val ? std::stod(val) : 0.0;
                invoke_guarded([&]() { it->second(v); });
            }
            return nullptr;
        }, this);
    }

    void register_void_callback(const std::string& name, std::function<void()> callback)
    {
        void_callback_map_[name] = callback;
        Tcl_CreateCommand(interp_, name.c_str(), [](ClientData client_data, Tcl_Interp*, int argc, const char* argv[]) -> int {
            auto* self = static_cast<Interpreter*>(client_data);
            auto it = self->void_callback_map_.find(argv[0]);
            if (it != self->void_callback_map_.end())
            {
                invoke_guarded([&]() { it->second(); });
            }
            return TCL_OK;
        }, this, nullptr);
    }

    void register_double_callback(const std::string& name, std::function<void(const double&)> callback)
    {
        double_callback_map_[name] = callback;
        Tcl_CreateCommand(interp_, name.c_str(), [](ClientData client_data, Tcl_Interp*, int argc, const char* argv[]) -> int {
            auto* self = static_cast<Interpreter*>(client_data);
            auto it = self->double_callback_map_.find(argv[0]);
            if (it != self->double_callback_map_.end())
            {
                invoke_guarded([&]() { it->second(safe_stod(argv[1])); });
            }
            return TCL_OK;
        }, this, nullptr);
    }

    void register_string_callback(const std::string& name, std::function<void(const std::string&)> callback)
    {
        string_callback_map_[name] = callback;
        Tcl_CreateCommand(interp_, name.c_str(), [](ClientData client_data, Tcl_Interp*, int argc, const char* argv[]) -> int {
            auto* self = static_cast<Interpreter*>(client_data);
            auto it = self->string_callback_map_.find(argv[0]);
            if (it != self->string_callback_map_.end())
            {
                std::string args;
                for (int i = 1; i < argc; ++i)
                {
                    if (i > 1) args += ' ';
                    args += argv[i];
                }
                invoke_guarded([&]() { it->second(args); });
            }
            return TCL_OK;
        }, this, nullptr);
    }

    void register_event_callback(const std::string& name, std::function<void(const Event&)> callback)
    {
        event_callback_map_[name] = callback;
        Tcl_CreateCommand(interp_, name.c_str(), [](ClientData client_data, Tcl_Interp*, int argc, const char* argv[]) -> int {
            auto* self = static_cast<Interpreter*>(client_data);
            auto it = self->event_callback_map_.find(argv[0]);
            if (it != self->event_callback_map_.end())
            {
                auto e      = Event();
                e.x         = safe_stol(argv[1]);
                e.y         = safe_stol(argv[2]);
                e.x_root    = safe_stol(argv[3]);
                e.y_root    = safe_stol(argv[4]);
                e.widget    = argv[5];
                e.keysym    = argv[6];
                e.keycode   = safe_stol(argv[7]);
                e.character = argv[8];
                e.type      = argv[9];
                e.delta     = safe_stol(argv[10]);
                invoke_guarded([&]() { it->second(e); });
            }
            return TCL_OK;
        }, this, nullptr);
    }

    // Entry::validate()等、Tcl側にbool(0/1)を返す必要があるコールバック(validatecommand等)用。
    // 他のregister_*_callbackと異なり、Tclコマンドの戻り値そのものをcallbackの結果にする。
    // コールバックが例外を投げた場合はfalse(編集拒否)側にfail closedする。
    void register_bool_callback(const std::string& name, std::function<bool(const std::string&)> callback)
    {
        bool_callback_map_[name] = callback;
        Tcl_CreateCommand(interp_, name.c_str(), [](ClientData client_data, Tcl_Interp* interp, int argc, const char* argv[]) -> int {
            auto* self = static_cast<Interpreter*>(client_data);
            auto it = self->bool_callback_map_.find(argv[0]);
            bool result = true;
            if (it != self->bool_callback_map_.end())
            {
                std::string arg = (argc > 1) ? argv[1] : "";
                bool ok = false;
                invoke_guarded([&]() { ok = it->second(arg); });
                result = ok;
            }
            Tcl_SetObjResult(interp, Tcl_NewBooleanObj(result ? 1 : 0));
            return TCL_OK;
        }, this, nullptr);
    }

private:
    // post()がTcl_ThreadQueueEventへ登録するイベント型。Tcl_Eventはヘッダ(header)を先頭に置いた
    // C互換構造体であることが要求されるため、std::function自体は埋め込まずヒープに確保して
    // ポインタだけを持たせる(Tcl_Alloc/Tcl_Freeで管理される領域にC++オブジェクトを直接
    // 構築/破棄する事態を避けるため)。
    struct PostedJobEvent
    {
        Tcl_Event               header;
        std::function<void()>*  job;
    };

    // Tcl_ThreadQueueEventで注入されたjobをTclのイベントループ上(=生成スレッド上)で実行する。
    // ここもTclのCコールスタックから直接呼ばれる境界なので、invoke_guarded経由で例外を握りつぶす。
    static int handle_posted_job(Tcl_Event* evPtr, int /*flags*/)
    {
        auto* pj = reinterpret_cast<PostedJobEvent*>(evPtr);
        invoke_guarded([&]() { (*pj->job)(); });
        delete pj->job;
        return 1; // 1 = 処理済み。呼び出し元(Tcl本体)がこの戻り値を見てキューから取り除く。
    }

    Tcl_Interp* interp_;

    std::thread::id owner_thread_;

    Tcl_ThreadId owner_tcl_thread_;

    std::unordered_map<std::string, std::function<void(const Event&)>>          event_callback_map_;

    std::unordered_map<std::string, std::function<void()>>                      void_callback_map_;

    std::unordered_map<std::string, std::function<void(const int&)>>            int_callback_map_;

    std::unordered_map<std::string, std::function<void(const double&)>>         double_callback_map_;

    std::unordered_map<std::string, std::function<bool(const std::string&)>>    bool_callback_map_;

    std::unordered_map<std::string, std::function<void(const std::string&)>>    string_callback_map_;
};

// Interpreterはスレッドごとにちょうど1つだけ存在する遅延生成のシングルトン。
// 呼び出しスレッドに紐づくInterpreterが無ければこの場で生成して登録するため、
// current_interp()は常に有効なポインタを返す(nullptrにはならない)。tk::Tkの構築を
// 待たずにVar/PhotoImage/font::Font/ttk::Styleを構築しても、その場でInterpreterが
// 用意される。同一スレッドでtk::Tkを複数回構築しても、この関数が返す既存の
// Interpreterを再利用するだけなので上書き・迷子になったInterpreterは発生しない。
Interpreter* current_interp()
{
    Interpreter*& interp = interp_map[std::this_thread::get_id()];
    if (!interp)
        interp = new Interpreter();
    return interp;
}

ArgValue::ArgValue()
    : type_(ValueType::NONE)
    , str_(nullptr)
    , bytes_(nullptr)
    , list_(nullptr)
    , dict_(nullptr)
{}

ArgValue::ArgValue(const std::string& s)
    : type_(ValueType::STRING)
    , str_(new std::string(s))
    , bytes_(nullptr)
    , list_(nullptr)
    , dict_(nullptr)
{}

ArgValue::ArgValue(const char* s)
    : type_(ValueType::STRING)
    , str_(new std::string(s))
    , bytes_(nullptr)
    , list_(nullptr)
    , dict_(nullptr)
{}

ArgValue::ArgValue(int v)
    : type_(ValueType::INT)
    , i_(v)
    , str_(nullptr)
    , bytes_(nullptr)
    , list_(nullptr)
    , dict_(nullptr)
{}

ArgValue::ArgValue(double v)
    : type_(ValueType::DOUBLE)
    , d_(v)
    , str_(nullptr)
    , bytes_(nullptr)
    , list_(nullptr)
    , dict_(nullptr)
{}

ArgValue::ArgValue(bool v)
    : type_(ValueType::BOOL)
    , b_(v)
    , str_(nullptr)
    , bytes_(nullptr)
    , list_(nullptr)
    , dict_(nullptr)
{}

ArgValue::ArgValue(const std::vector<uint8_t>& bytes)
    : type_(ValueType::BYTES)
    , str_(nullptr)
    , bytes_(new std::vector<uint8_t>(bytes))
    , list_(nullptr)
    , dict_(nullptr)
{}

ArgValue::ArgValue(const std::vector<ArgValue>& list)
    : type_(ValueType::LIST)
    , str_(nullptr)
    , bytes_(nullptr)
    , list_(new std::vector<ArgValue>(list))
    , dict_(nullptr)
{}

ArgValue::ArgValue(const std::map<std::string, ArgValue>& dict)
    : type_(ValueType::DICT)
    , str_(nullptr)
    , bytes_(nullptr)
    , list_(nullptr)
    , dict_(new std::map<std::string, ArgValue>(dict))
{}

ArgValue::ArgValue(Var& var)
    : ArgValue(var.name())
{}

ArgValue::ArgValue(const ArgValue& other)
    : type_(ValueType::NONE)
    , str_(nullptr)
    , bytes_(nullptr)
    , list_(nullptr)
    , dict_(nullptr)
{
    copy_from(other);
}

ArgValue& ArgValue::operator=(const ArgValue& other)
{
    if (this != &other)
    {
        cleanup();
        copy_from(other);
    }
    return *this;
}

ArgValue::~ArgValue()
{
    cleanup();
}

ArgValue::ValueType ArgValue::type() const
{
    return type_;
}

void ArgValue::cleanup()
{
    if (type_ == ValueType::STRING && str_)
    {
        delete str_;
        str_ = nullptr;
    }
    else if (type_ == ValueType::BYTES && bytes_)
    {
        delete bytes_;
        bytes_ = nullptr;
    }
    else if (type_ == ValueType::LIST && list_)
    {
        delete list_;
        list_ = nullptr;
    }
    else if (type_ == ValueType::DICT && dict_)
    {
        delete dict_;
        dict_ = nullptr;
    }
    type_ = ValueType::NONE;
}

void ArgValue::copy_from(const ArgValue& other)
{
    type_ = other.type_;
    str_  = nullptr;
    bytes_ = nullptr;
    list_  = nullptr;
    dict_  = nullptr;

    if (other.type_ == ValueType::STRING)
    {
        str_ = new std::string(*other.str_);
    }
    else if (other.type_ == ValueType::INT)
    {
        i_ = other.i_;
    }
    else if (other.type_ == ValueType::DOUBLE)
    {
        d_ = other.d_;
    }
    else if (other.type_ == ValueType::BOOL)
    {
        b_ = other.b_;
    }
    else if (other.type_ == ValueType::BYTES)
    {
        bytes_ = new std::vector<uint8_t>(*other.bytes_);
    }
    else if (other.type_ == ValueType::LIST)
    {
        list_ = new std::vector<ArgValue>(*other.list_);
    }
    else if (other.type_ == ValueType::DICT)
    {
        dict_ = new std::map<std::string, ArgValue>(*other.dict_);
    }
}

ArgValue list(std::initializer_list<ArgValue> items)
{
    return ArgValue(std::vector<ArgValue>(items));
}

ArgValue dict(std::initializer_list<std::pair<const std::string, ArgValue>> items)
{
    return ArgValue(std::map<std::string, ArgValue>(items));
}

Object::Object()
    : id(next())
{}

std::string Object::next()
{
    static int count = 0;
    return std::to_string(count++);
}

Interpreter* InterpreterClient::checked_interp(const char* operation, bool* success) const
{
    auto* p = interp();
    if (p == nullptr)
    {
        report_or_throw(std::string(operation) + "() called on an uninitialized " + type_name() + " (interp == nullptr).", success, ErrorPolicy::LENIENT_CALL);
        return nullptr;
    }

    // Tcl_Interpは生成したスレッド以外から触ると内部状態を破壊しうる未定義動作になりうるが、
    // この検知自体はTcl_Interpに一切触れる前に働くため、Error送出/ログのみ継続のどちらを
    // 選んでも「危険なTcl呼び出しをしない」という安全性は同じ(呼び出し元への通知方法が
    // 違うだけ)。そのためLENIENT_THREADで他のカテゴリと同様に緩められるようにする。
    if (p->owner_thread() != std::this_thread::get_id())
    {
        report_or_throw(std::string(operation) + "() called on a " + type_name()
            + " from a thread different than the one that owns its Tcl interpreter. "
              "Tcl_Interp must only be accessed from the thread that created it.", success, ErrorPolicy::LENIENT_THREAD);
        return nullptr;
    }

    return p;
}

std::string InterpreterClient::call(const std::vector<ArgValue>& words, bool* success) const
{
    auto* p = checked_interp("call", success);
    if (p == nullptr)
        return {};

    bool ok = false;
    auto result = p->call(words, &ok);
    if (!ok)
    {
        report_or_throw("call() failed to execute Tcl command: " + result, success, ErrorPolicy::LENIENT_CALL);
        return result;
    }
    if (success) *success = true;
    return result;
}

void InterpreterClient::post(std::function<void()> job) const
{
    // checked_interp()は意図的に通さない(スレッド一致チェックはpost()の用途と矛盾するため)。
    // 未初期化オブジェクトへの呼び出しはこれまで通りerror_policy()に従う(この時点ではまだ
    // Tcl_Interpに一切触れていないため、クロススレッドの危険はない)。
    auto* p = interp();
    if (p == nullptr)
    {
        report_or_throw(std::string("post() called on an uninitialized ") + type_name() + " (interp == nullptr).", nullptr, ErrorPolicy::LENIENT_CALL);
        return;
    }
    p->post(std::move(job));
}

Var::Var()
    : interp_(nullptr)
{}

const std::string& Var::name() const
{
    return name_;
}

std::string Var::get_var() const
{
    auto* p = checked_interp("get_var");
    return p ? p->get_var(name_) : std::string{};
}

void Var::set_var(const std::string& value)
{
    auto* p = checked_interp("set_var");
    if (p) p->set_var(name_, value);
}

void Var::trace_var(std::function<void(const std::string&)> callback)
{
    auto* p = checked_interp("trace");
    if (p) p->trace_var(name_, callback);
}

void Var::trace_var(std::function<void(const int&)> callback)
{
    auto* p = checked_interp("trace");
    if (p) p->trace_var(name_, callback);
}

void Var::trace_var(std::function<void(const double&)> callback)
{
    auto* p = checked_interp("trace");
    if (p) p->trace_var(name_, callback);
}

StringVar::StringVar()
{
    interp_ = current_interp();
    name_ = "string_var_" + id;
    set_var("");
}

void StringVar::set(const std::string &value)
{
    set_var(value);
}

std::string StringVar::get() const
{
    return get_var();
}

void StringVar::trace(std::function<void(const std::string&)> callback)
{
    trace_var(callback);
}

BooleanVar::BooleanVar()
{
    interp_ = current_interp();
    name_ = "bool_var_" + id;
    set_var("0");
}

void BooleanVar::set(bool value)
{
    set_var(value ? "1" : "0");
}

bool BooleanVar::get() const
{
    return get_var() == "1";
}

void BooleanVar::trace(std::function<void(const bool&)> callback)
{
    trace_var([callback](const std::string& v){
        callback(v == "1");
    });
}

IntVar::IntVar()
{
    interp_ = current_interp();
    name_ = "int_var_" + id;
    set_var("0");
}

void IntVar::set(const int& value)
{
    set_var(std::to_string(value));
}

int IntVar::get() const
{
    return safe_stol(get_var().c_str());
}

void IntVar::trace(std::function<void(const int&)> callback)
{
    trace_var(callback);
}

DoubleVar::DoubleVar()
{
    interp_ = current_interp();
    name_ = "double_var_" + id;
    set_var("0.0");
}

void DoubleVar::set(const double& value)
{
    set_var(std::to_string(value));
}

double DoubleVar::get() const
{
    return safe_stod(get_var().c_str());
}

void DoubleVar::trace(std::function<void(const double&)> callback)
{
    trace_var(callback);
}

Widget::Widget()
{}

Widget::Widget(const Widget& parent, const std::string &type, const std::string& name, const std::map<std::string, ArgValue>& options)
{
    impl_->interp = parent.impl_->interp;
    auto parent_name = (parent.full_name() == ".") ? "" : parent.full_name();
    impl_->full_name = parent_name + "." + (name.empty() ? type : name) + id;

    std::vector<ArgValue> words = {type, impl_->full_name};
    for (const auto& kv : options)
    {
        words.push_back("-" + kv.first);
        words.push_back(kv.second);
    }
    call(words);
}

const std::string& Widget::full_name() const
{
    return impl_->full_name;
}

void Widget::register_void_callback(const std::string& name, std::function<void()> callback) const
{
    auto* p = checked_interp("register_void_callback");
    if (p) p->register_void_callback(name, callback);
}

void Widget::register_string_callback(const std::string& name, std::function<void(const std::string&)> callback) const
{
    auto* p = checked_interp("register_string_callback");
    if (p) p->register_string_callback(name, callback);
}

void Widget::register_double_callback(const std::string& name, std::function<void(const double&)> callback) const
{
    auto* p = checked_interp("register_double_callback");
    if (p) p->register_double_callback(name, callback);
}

void Widget::register_event_callback(const std::string& name, std::function<void(const Event&)> callback) const
{
    auto* p = checked_interp("register_event_callback");
    if (p) p->register_event_callback(name, callback);
}

void Widget::register_bool_callback(const std::string& name, std::function<bool(const std::string&)> callback) const
{
    auto* p = checked_interp("register_bool_callback");
    if (p) p->register_bool_callback(name, callback);
}

Widget& Widget::pack(const std::map<std::string, ArgValue> &options)
{
    std::vector<ArgValue> words = {"pack", impl_->full_name};
    for (const auto& kv : options)
    {
        words.push_back("-" + kv.first);
        words.push_back(kv.second);
    }
    call(words);
    return *this;
}

Widget& Widget::pack_forget()
{
    call({"pack", "forget", impl_->full_name});
    return *this;
}

Widget& Widget::grid(const std::map<std::string, ArgValue>& options)
{
    std::vector<ArgValue> words = {"grid", impl_->full_name};
    for (const auto& kv : options)
    {
        words.push_back("-" + kv.first);
        words.push_back(kv.second);
    }
    call(words);
    return *this;
}

Widget& Widget::grid_forget()
{
    call({"grid", "forget", impl_->full_name});
    return *this;
}

Widget& Widget::place(const std::map<std::string, ArgValue> &options)
{
    std::vector<ArgValue> words = {"place", impl_->full_name};
    for (const auto& kv : options)
    {
        words.push_back("-" + kv.first);
        words.push_back(kv.second);
    }
    call(words);
    return *this;
}

Widget& Widget::place_forget()
{
    call({"place", "forget", impl_->full_name});
    return *this;
}

Widget& Widget::config(const std::map<std::string, ArgValue> &options)
{
    if (options.empty())
    {
        return *this;
    }

    std::vector<ArgValue> words = {impl_->full_name, "configure"};
    for (const auto &kv : options)
    {
        words.push_back("-" + kv.first);
        words.push_back(kv.second);
    }
    call(words);
    return *this;
}

Widget& Widget::config(const std::string& name, const ArgValue& value)
{
    config({{name, value}});
    return *this;
}

Widget& Widget::grid_rowconfigure(int row, const std::map<std::string, ArgValue>& options)
{
    std::vector<ArgValue> words = {"grid", "rowconfigure", impl_->full_name, row};
    for (const auto& kv : options)
    {
        words.push_back("-" + kv.first);
        words.push_back(kv.second);
    }
    call(words);
    return *this;
}

Widget& Widget::grid_columnconfigure(int column, const std::map<std::string, ArgValue>& options)
{
    std::vector<ArgValue> words = {"grid", "columnconfigure", impl_->full_name, column};
    for (const auto& kv : options)
    {
        words.push_back("-" + kv.first);
        words.push_back(kv.second);
    }
    call(words);
    return *this;
}

std::string Widget::cget(const std::string& name) const
{
    auto ok  = false;
    auto ret = call({impl_->full_name, "cget", "-" + name}, &ok);
    if (!ok)
        ret = "";
    return ret;
}

Widget& Widget::bind(const std::string& event, std::function<void(const Event&)> callback)
{
    auto cb_name = sanitize(full_name()) + "_" + sanitize(event) + "_bind_cb";
    register_event_callback(cb_name, callback);
    std::string script = cb_name + " %x %y %X %Y %W %K %k %c %t %D";
    call({"bind", impl_->full_name, event, script});
    return *this;
}

Widget& Widget::unbind(const std::string& event)
{
    call({"bind", impl_->full_name, event, ""});
    return *this;
}

Widget& Widget::bind_all(const std::string& event, std::function<void(const Event&)> callback)
{
    auto cb_name = "all_" + sanitize(event) + "_bindall_cb";
    register_event_callback(cb_name, callback);
    std::string script = cb_name + " %x %y %X %Y %W %K %k %c %t %D";
    call({"bind", "all", event, script});
    return *this;
}

Widget& Widget::unbind_all(const std::string& event)
{
    call({"bind", "all", event, ""});
    return *this;
}

Widget& Widget::bind_class(const std::string& class_name, const std::string& event, std::function<void(const Event&)> callback)
{
    auto cb_name = sanitize(class_name) + "_" + sanitize(event) + "_bindclass_cb";
    register_event_callback(cb_name, callback);
    std::string script = cb_name + " %x %y %X %Y %W %K %k %c %t %D";
    call({"bind", class_name, event, script});
    return *this;
}

std::string Widget::after(const int& ms, std::function<void()> callback)
{
    auto cb_name = sanitize(full_name()) + "_after_cb_" + std::to_string(impl_->after_id++);
    register_void_callback(cb_name, callback);
    auto ok  = false;
    auto ret = call({"after", ms, cb_name}, &ok);
    return ret;
}

void Widget::after_idle(std::function<void()> callback)
{
    auto cb_name = sanitize(full_name()) + "_after_idle_cb";
    register_void_callback(cb_name, callback);
    call({"after", "idle", cb_name});
}

void Widget::after_cancel(const std::string& id)
{
    call({"after", "cancel", id});
}

void Widget::destroy()
{
    call({"destroy", impl_->full_name});
}

void Widget::update()
{
    call({"update"});
}

void Widget::update_idletasks()
{
    call({"update", "idletasks"});
}

void Widget::event_generate(const std::string& event, const std::map<std::string, ArgValue>& options)
{
    std::vector<ArgValue> words = {"event", "generate", impl_->full_name, event};
    for (const auto& kv : options)
    {
        words.push_back("-" + kv.first);
        words.push_back(kv.second);
    }
    call(words);
}

int Widget::winfo_width() const
{
    auto ret = call({"winfo", "width", impl_->full_name});
    return safe_stol(ret.c_str());
}

int Widget::winfo_height() const
{
    auto ret = call({"winfo", "height", impl_->full_name});
    return safe_stol(ret.c_str());
}

int Widget::winfo_reqwidth() const
{
    auto ret = call({"winfo", "reqwidth", impl_->full_name});
    return safe_stol(ret.c_str());
}

int Widget::winfo_reqheight() const
{
    auto ret = call({"winfo", "reqheight", impl_->full_name});
    return safe_stol(ret.c_str());
}

int Widget::winfo_x() const
{
    auto ret = call({"winfo", "x", impl_->full_name});
    return safe_stol(ret.c_str());
}

int Widget::winfo_y() const
{
    auto ret = call({"winfo", "y", impl_->full_name});
    return safe_stol(ret.c_str());
}

int Widget::winfo_rootx() const
{
    auto ret = call({"winfo", "rootx", impl_->full_name});
    return safe_stol(ret.c_str());
}

int Widget::winfo_rooty() const
{
    auto ret = call({"winfo", "rooty", impl_->full_name});
    return safe_stol(ret.c_str());
}

bool Widget::winfo_exists() const
{
    auto ret = call({"winfo", "exists", impl_->full_name});
    return safe_stol(ret.c_str()) != 0;
}

std::string Widget::winfo_class() const
{
    return call({"winfo", "class", impl_->full_name});
}

std::string Widget::winfo_toplevel() const
{
    return call({"winfo", "toplevel", impl_->full_name});
}

std::vector<std::string> Widget::winfo_children() const
{
    auto result = call({"winfo", "children", impl_->full_name});
    return impl_->interp->split_list(result);
}

Widget Widget::nametowidget(const std::string& name) const
{
    auto impl = std::make_shared<Impl>();
    impl->interp = impl_->interp;
    impl->full_name = name;
    return Widget(impl);
}

std::vector<std::string> Widget::grid_slaves() const
{
    auto result = call({"grid", "slaves", impl_->full_name});
    return impl_->interp->split_list(result);
}

std::vector<std::string> Widget::pack_slaves() const
{
    auto result = call({"pack", "slaves", impl_->full_name});
    return impl_->interp->split_list(result);
}

std::vector<std::string> Widget::place_slaves() const
{
    auto result = call({"place", "slaves", impl_->full_name});
    return impl_->interp->split_list(result);
}

// grid/pack/place infoの共通実装。戻り値は"-key1 val1 -key2 val2 ..."形式なので、
// split_listでトークン化してから2個1組でmapに詰め直す。
static std::map<std::string, std::string> parse_geometry_info(Interpreter& interp, const std::string& raw)
{
    std::map<std::string, std::string> result;
    auto tokens = interp.split_list(raw);
    for (std::size_t i = 0; i + 1 < tokens.size(); i += 2)
    {
        std::string key = tokens[i];
        if (!key.empty() && key[0] == '-')
            key = key.substr(1);
        result[key] = tokens[i + 1];
    }
    return result;
}

std::map<std::string, std::string> Widget::grid_info() const
{
    bool ok = false;
    auto result = call({"grid", "info", impl_->full_name}, &ok);
    if (!ok) return {};
    return parse_geometry_info(*impl_->interp, result);
}

std::map<std::string, std::string> Widget::pack_info() const
{
    bool ok = false;
    auto result = call({"pack", "info", impl_->full_name}, &ok);
    if (!ok) return {};
    return parse_geometry_info(*impl_->interp, result);
}

std::map<std::string, std::string> Widget::place_info() const
{
    bool ok = false;
    auto result = call({"place", "info", impl_->full_name}, &ok);
    if (!ok) return {};
    return parse_geometry_info(*impl_->interp, result);
}

int Widget::winfo_screenwidth() const
{
    auto ret = call({"winfo", "screenwidth", impl_->full_name});
    return safe_stol(ret.c_str());
}

int Widget::winfo_screenheight() const
{
    auto ret = call({"winfo", "screenheight", impl_->full_name});
    return safe_stol(ret.c_str());
}

int Widget::winfo_pointerx() const
{
    auto ret = call({"winfo", "pointerx", impl_->full_name});
    return safe_stol(ret.c_str());
}

int Widget::winfo_pointery() const
{
    auto ret = call({"winfo", "pointery", impl_->full_name});
    return safe_stol(ret.c_str());
}

std::string Widget::winfo_manager() const
{
    return call({"winfo", "manager", impl_->full_name});
}

bool Widget::winfo_ismapped() const
{
    auto ret = call({"winfo", "ismapped", impl_->full_name});
    return safe_stol(ret.c_str()) != 0;
}

Widget& Widget::focus_set()
{
    call({"focus", impl_->full_name});
    return *this;
}

Widget& Widget::focus_force()
{
    call({"focus", "-force", impl_->full_name});
    return *this;
}

std::string Widget::focus_get() const
{
    auto ok  = false;
    auto ret = call({"focus"}, &ok);
    return ok ? ret : "";
}

void Widget::clipboard_clear()
{
    call({"clipboard", "clear"});
}

void Widget::clipboard_append(const std::string& text)
{
    call({"clipboard", "append", text});
}

std::string Widget::clipboard_get() const
{
    auto ok  = false;
    auto ret = call({"clipboard", "get"}, &ok);
    return ok ? ret : "";
}

void Widget::wait_window() const
{
    call({"tkwait", "window", impl_->full_name});
}

void Widget::wait_variable(const Var& var) const
{
    call({"tkwait", "variable", var.name()});
}

void Widget::wait_visibility() const
{
    call({"tkwait", "visibility", impl_->full_name});
}

Widget& Widget::lift()
{
    call({"raise", impl_->full_name});
    return *this;
}

Widget& Widget::lower()
{
    call({"lower", impl_->full_name});
    return *this;
}

std::string Widget::windowingsystem() const
{
    return call({"tk", "windowingsystem"});
}

void Widget::bell()
{
    call({"bell"});
}

double Widget::scaling() const
{
    auto ret = call({"tk", "scaling"});
    return safe_stod(ret.c_str());
}

Widget& Widget::scaling(double factor)
{
    call({"tk", "scaling", factor});
    return *this;
}

std::vector<std::string> Widget::bindtags() const
{
    auto ret = call({"bindtags", impl_->full_name});
    return impl_->interp->split_list(ret);
}

Widget& Widget::bindtags(const std::vector<std::string>& tags)
{
    call({"bindtags", impl_->full_name, std::vector<ArgValue>(tags.begin(), tags.end())});
    return *this;
}

std::string Widget::grab_current() const
{
    bool ok = false;
    auto ret = call({"grab", "current", impl_->full_name}, &ok);
    if (!ok) return "";
    return ret;
}

std::string Widget::grab_status() const
{
    return call({"grab", "status", impl_->full_name});
}

std::string Widget::winfo_id() const
{
    return call({"winfo", "id", impl_->full_name});
}

std::string Widget::winfo_name() const
{
    return call({"winfo", "name", impl_->full_name});
}

std::string Widget::winfo_parent() const
{
    return call({"winfo", "parent", impl_->full_name});
}

int Widget::winfo_depth() const
{
    return safe_stol(call({"winfo", "depth", impl_->full_name}).c_str());
}

std::string Widget::winfo_geometry() const
{
    return call({"winfo", "geometry", impl_->full_name});
}

std::string Widget::winfo_containing(int root_x, int root_y) const
{
    bool ok = false;
    auto ret = call({"winfo", "containing", "-displayof", impl_->full_name, root_x, root_y}, &ok);
    if (!ok) return "";
    return ret;
}

void Widget::option_add(const std::string& pattern, const std::string& value, const std::string& priority)
{
    if (priority.empty())
        call({"option", "add", pattern, value});
    else
        call({"option", "add", pattern, value, priority});
}

std::string Widget::option_get(const std::string& name, const std::string& class_name) const
{
    bool ok = false;
    auto ret = call({"option", "get", impl_->full_name, name, class_name}, &ok);
    if (!ok) return "";
    return ret;
}

Widget Widget::tk_focusNext() const
{
    auto ret = call({"tk_focusNext", impl_->full_name});
    return nametowidget(ret);
}

Widget Widget::tk_focusPrev() const
{
    auto ret = call({"tk_focusPrev", impl_->full_name});
    return nametowidget(ret);
}

PhotoImage::PhotoImage(const std::map<std::string, ArgValue>& options)
    : interp_(current_interp())
{
    name_ = "img_" + id;
    std::vector<ArgValue> words = {"image", "create", "photo", name_};
    for (const auto& kv : options)
    {
        words.push_back("-" + kv.first);
        words.push_back(kv.second);
    }

    call(words);
}

const std::string& PhotoImage::name() const
{
    return name_;
}

void PhotoImage::destroy()
{
    call({"image", "delete", name_});
}

PhotoImage& PhotoImage::put(const std::string& data, const std::map<std::string, ArgValue>& options)
{
    std::vector<ArgValue> words = {name_, "put", data};
    for (const auto& kv : options)
    {
        words.push_back("-" + kv.first);
        words.push_back(kv.second);
    }
    call(words);
    return *this;
}

std::string PhotoImage::get(int x, int y) const
{
    return call({name_, "get", x, y});
}

PhotoImage& PhotoImage::blank()
{
    call({name_, "blank"});
    return *this;
}

PhotoImage& PhotoImage::copy_from(const PhotoImage& source, const std::map<std::string, ArgValue>& options)
{
    std::vector<ArgValue> words = {name_, "copy", source.name_};
    for (const auto& kv : options)
    {
        words.push_back("-" + kv.first);
        words.push_back(kv.second);
    }
    call(words);
    return *this;
}

PhotoImage PhotoImage::copy() const
{
    // デフォルト引数のPhotoImage()はcurrent_interp()に束縛された空のphotoイメージを既に
    // 生成済みのため、明示的な"image create photo"の再呼び出しは不要。
    PhotoImage result;
    call({result.name_, "copy", name_});
    return result;
}

PhotoImage PhotoImage::zoom(int x, int y) const
{
    PhotoImage result;
    if (y > 0)
        call({result.name_, "copy", name_, "-zoom", x, y});
    else
        call({result.name_, "copy", name_, "-zoom", x});
    return result;
}

PhotoImage PhotoImage::subsample(int x, int y) const
{
    PhotoImage result;
    if (y > 0)
        call({result.name_, "copy", name_, "-subsample", x, y});
    else
        call({result.name_, "copy", name_, "-subsample", x});
    return result;
}

int PhotoImage::width() const
{
    return safe_stol(call({"image", "width", name_}).c_str());
}

int PhotoImage::height() const
{
    return safe_stol(call({"image", "height", name_}).c_str());
}

BitmapImage::BitmapImage(const std::map<std::string, ArgValue>& options)
    : interp_(current_interp())
{
    name_ = "bmp_" + id;
    std::vector<ArgValue> words = {"image", "create", "bitmap", name_};
    for (const auto& kv : options)
    {
        words.push_back("-" + kv.first);
        words.push_back(kv.second);
    }
    call(words);
}

const std::string& BitmapImage::name() const
{
    return name_;
}

void BitmapImage::destroy()
{
    call({"image", "delete", name_});
}

int BitmapImage::width() const
{
    return safe_stol(call({"image", "width", name_}).c_str());
}

int BitmapImage::height() const
{
    return safe_stol(call({"image", "height", name_}).c_str());
}

std::vector<std::string> image_names()
{
    auto* interp = current_interp();
    bool ok = false;
    auto ret = interp->call({"image", "names"}, &ok);
    if (!ok) return {};
    return interp->split_list(ret);
}

std::vector<std::string> image_types()
{
    auto* interp = current_interp();
    bool ok = false;
    auto ret = interp->call({"image", "types"}, &ok);
    if (!ok) return {};
    return interp->split_list(ret);
}

Tk::Tk()
    : Widget()
{
    impl_->interp = current_interp();
    impl_->full_name = ".";

    title("tk");
    geometry("300x300");
    auto self_handle = handle();
    protocol("WM_DELETE_WINDOW", [self_handle](){ Tk(self_handle).quit(); });
}

Tk& Tk::title(const std::string& title)
{
    call({"wm", "title", ".", title});
    return *this;
}

Tk& Tk::geometry(const std::string &size)
{
    call({"wm", "geometry", ".", size});
    return *this;
}

std::string Tk::geometry() const
{
    return call({"wm", "geometry", "."});
}

Tk& Tk::protocol(const std::string& name, std::function<void()> handler) 
{
    auto cb_name = "protocol_cb_" + sanitize(name);
    register_void_callback(cb_name, handler);
    call({"wm", "protocol", ".", name, cb_name});
    return *this;
}

Tk& Tk::resizable(bool width, bool height)
{
    call({"wm", "resizable", ".", (int)(width ? 1 : 0), (int)(height ? 1 : 0)});
    return *this;
}

Tk& Tk::overrideredirect(bool value)
{
    call({"wm", "overrideredirect", ".", (int)(value ? 1 : 0)});
    return *this;
}

bool Tk::overrideredirect() const
{
    auto ret = call({"wm", "overrideredirect", "."});
    return safe_stol(ret.c_str()) != 0;
}

Tk& Tk::minsize(int width, int height)
{
    call({"wm", "minsize", ".", width, height});
    return *this;
}

Tk& Tk::maxsize(int width, int height)
{
    call({"wm", "maxsize", ".", width, height});
    return *this;
}

Tk& Tk::iconify()
{
    call({"wm", "iconify", "."});
    return *this;
}

Tk& Tk::deiconify()
{
    call({"wm", "deiconify", "."});
    return *this;
}

Tk& Tk::withdraw()
{
    call({"wm", "withdraw", "."});
    return *this;
}

Tk& Tk::state(const std::string& new_state)
{
    call({"wm", "state", ".", new_state});
    return *this;
}

std::string Tk::state() const
{
    return call({"wm", "state", "."});
}

Tk& Tk::attributes(const std::string& name, const std::string& value)
{
    call({"wm", "attributes", ".", name, value});
    return *this;
}

std::string Tk::attributes(const std::string& name) const
{
    return call({"wm", "attributes", ".", name});
}

Tk& Tk::lift()
{
    call({"raise", "."});
    return *this;
}

Tk& Tk::lower()
{
    call({"lower", "."});
    return *this;
}

Tk& Tk::grab_set()
{
    call({"grab", "set", "."});
    return *this;
}

Tk& Tk::grab_release()
{
    call({"grab", "release", "."});
    return *this;
}

Tk& Tk::iconphoto(const std::string& image_name)
{
    call({"wm", "iconphoto", ".", "-default", image_name});
    return *this;
}

Tk& Tk::iconbitmap(const std::string& bitmap_path)
{
    call({"wm", "iconbitmap", ".", bitmap_path});
    return *this;
}

Tk& Tk::iconname(const std::string& name)
{
    call({"wm", "iconname", ".", name});
    return *this;
}

std::string Tk::iconname() const
{
    return call({"wm", "iconname", "."});
}

void Tk::mainloop()
{
    call({"vwait", "forever"});
}

void Tk::quit() 
{
    call({"set", "forever", 1});
}

Checkbutton::Checkbutton(const Widget& parent, const std::map<std::string, ArgValue>& options)
    : Widget(parent, "checkbutton", "chk", options)
{
}

Checkbutton& Checkbutton::text(const std::string& text)
{
    config({{"text", text}});
    return *this;
}

Checkbutton& Checkbutton::variable(Var& var)
{
    config({{"variable", var}});
    return *this;
}

Checkbutton& Checkbutton::command(std::function<void()> callback)
{
    auto cb = sanitize(full_name()) + "_chk_cb";
    register_void_callback(cb, callback);
    config({{"command", cb}});
    return *this;
}

std::string Checkbutton::invoke()
{
    return Widget::call({impl_->full_name, "invoke"});
}

Frame::Frame(const Widget& parent, const std::map<std::string, ArgValue>& options)
    : Widget(parent, "frame", "f", options)
{
}

Frame& Frame::width(const int &width)
{
    config({{"width", std::to_string(width)}});
    return *this;
}

Frame& Frame::height(const int &height)
{
    config({{"height", std::to_string(height)}});
    return *this;
}

Frame& Frame::grid_propagate(const bool& value)
{
    call({"grid", "propagate", impl_->full_name, (int)(value ? 1 : 0)});
    return *this;
}

Toplevel::Toplevel(const Widget& parent, const std::map<std::string, ArgValue>& options)
    : Widget(parent, "toplevel", "toplevel", options)
{
    auto self_handle = handle();
    protocol("WM_DELETE_WINDOW", [self_handle](){ Widget(self_handle).destroy(); });
}

Toplevel& Toplevel::title(const std::string &title_text)
{
    call({"wm", "title", impl_->full_name, title_text});
    return *this;
}

Toplevel& Toplevel::geometry(const std::string &size)
{
    call({"wm", "geometry", impl_->full_name, size});
    return *this;
}

std::string Toplevel::geometry() const
{
    return call({"wm", "geometry", impl_->full_name});
}

Toplevel& Toplevel::protocol(const std::string& name, std::function<void()> handler) 
{
    std::string callback_name = "protocol_cb_" + sanitize(full_name()) + "_" + sanitize(name);
    register_void_callback(callback_name, handler);
    call({"wm", "protocol", impl_->full_name, name, callback_name});
    return *this;
}

Toplevel& Toplevel::resizable(bool width, bool height)
{
    call({"wm", "resizable", impl_->full_name, (int)(width ? 1 : 0), (int)(height ? 1 : 0)});
    return *this;
}

Toplevel& Toplevel::overrideredirect(bool value)
{
    call({"wm", "overrideredirect", impl_->full_name, (int)(value ? 1 : 0)});
    return *this;
}

bool Toplevel::overrideredirect() const
{
    auto ret = call({"wm", "overrideredirect", impl_->full_name});
    return safe_stol(ret.c_str()) != 0;
}

Toplevel& Toplevel::minsize(int width, int height)
{
    call({"wm", "minsize", impl_->full_name, width, height});
    return *this;
}

Toplevel& Toplevel::maxsize(int width, int height)
{
    call({"wm", "maxsize", impl_->full_name, width, height});
    return *this;
}

Toplevel& Toplevel::attributes(const std::string& name, const std::string& value)
{
    call({"wm", "attributes", impl_->full_name, name, value});
    return *this;
}

std::string Toplevel::attributes(const std::string& name) const
{
    return call({"wm", "attributes", impl_->full_name, name});
}

Toplevel& Toplevel::iconify()
{
    call({"wm", "iconify", impl_->full_name});
    return *this;
}

Toplevel& Toplevel::deiconify()
{
    call({"wm", "deiconify", impl_->full_name});
    return *this;
}

Toplevel& Toplevel::withdraw()
{
    call({"wm", "withdraw", impl_->full_name});
    return *this;
}

Toplevel& Toplevel::state(const std::string& new_state)
{
    call({"wm", "state", impl_->full_name, new_state});
    return *this;
}

std::string Toplevel::state() const
{
    return call({"wm", "state", impl_->full_name});
}

Toplevel& Toplevel::lift()
{
    call({"raise", impl_->full_name});
    return *this;
}

Toplevel& Toplevel::lower()
{
    call({"lower", impl_->full_name});
    return *this;
}

Toplevel& Toplevel::grab_set()
{
    call({"grab", "set", impl_->full_name});
    return *this;
}

Toplevel& Toplevel::grab_release()
{
    call({"grab", "release", impl_->full_name});
    return *this;
}

Toplevel& Toplevel::iconphoto(const std::string& image_name)
{
    call({"wm", "iconphoto", impl_->full_name, "-default", image_name});
    return *this;
}

Toplevel& Toplevel::iconbitmap(const std::string& bitmap_path)
{
    call({"wm", "iconbitmap", impl_->full_name, bitmap_path});
    return *this;
}

Toplevel& Toplevel::iconname(const std::string& name)
{
    call({"wm", "iconname", impl_->full_name, name});
    return *this;
}

std::string Toplevel::iconname() const
{
    return call({"wm", "iconname", impl_->full_name});
}

Toplevel& Toplevel::transient(const Widget& master)
{
    call({"wm", "transient", impl_->full_name, master.full_name()});
    return *this;
}

Button::Button(const Widget& parent, const std::map<std::string, ArgValue>& options)
    : Widget(parent, "button", "b", options)
{
}

Button& Button::width(const int& width)
{
    config({{"width",  std::to_string(width)}});
    return *this;
}

Button& Button::height(const int& height)
{
    config({{"height",  std::to_string(height)}});
    return *this;
}

Button& Button::text(const std::string& text)
{
    config({{"text", text}});        
    return *this;
}

Button& Button::command(std::function<void()> callback)
{
    std::string callback_name =  sanitize(full_name()) + "_void_cb";
    register_void_callback(callback_name, callback);
    config({{"command", callback_name}});
    return *this;
}

std::string Button::invoke()
{
    return Widget::call({impl_->full_name, "invoke"});
}

Canvas::Canvas(const Widget& parent, const std::map<std::string, ArgValue>& options)
    : Widget(parent, "canvas", "c", options)
{
}

Canvas& Canvas::itemconfig(const std::string& id_or_tag, const std::map<std::string, ArgValue>& options)
{
    std::vector<ArgValue> words = {impl_->full_name, "itemconfigure", id_or_tag};
    for (const auto &kv : options)
    {
        words.push_back("-" + kv.first);
        words.push_back(kv.second);
    }
    call(words);
    return *this;
}

std::string Canvas::itemcget(const std::string& id_or_tag, const std::string& option) const
{
    auto ok  = false;
    auto ret = call({impl_->full_name, "itemcget", id_or_tag, "-" + option}, &ok);
    return ok ? ret : "";
}

std::vector<int> Canvas::bbox(const std::string& id_or_tag) const
{
    auto ok     = false;
    auto result = call({impl_->full_name, "bbox", id_or_tag}, &ok);

    std::vector<int> box;
    if (!ok)
        return box;

    for (const auto& token : impl_->interp->split_list(result))
        box.push_back(safe_stol(token.c_str()));
    return box;
}

Canvas& Canvas::tag_bind(const std::string& id_or_tag, const std::string& event, std::function<void(const Event&)> callback)
{
    auto cb_name = sanitize(full_name()) + "_tag_" + sanitize(id_or_tag) + "_" + sanitize(event);
    register_event_callback(cb_name, callback);
    std::string script = cb_name + " %x %y %X %Y %W %K %k %c %t %D";
    call({impl_->full_name, "bind", id_or_tag, event, script});
    return *this;
}

Canvas& Canvas::tag_unbind(const std::string& id_or_tag, const std::string& event)
{
    call({impl_->full_name, "bind", id_or_tag, event, ""});
    return *this;
}

std::string Canvas::create_line(const std::vector<std::array<double, 2>>& points, const std::map<std::string, ArgValue>& options)
{
    std::vector<ArgValue> words = {impl_->full_name, "create", "line"};
    for (const auto& pt : points)
    {
        words.push_back(pt[0]);
        words.push_back(pt[1]);
    }
    for (const auto &kv : options)
    {
        words.push_back("-" + kv.first);
        words.push_back(kv.second);
    }
    return call(words);
}

std::string Canvas::create_line(const int& x1, const int& y1, const int& x2, const int& y2, const std::map<std::string, ArgValue>& options)
{
    std::vector<ArgValue> words = {impl_->full_name, "create", "line", x1, y1, x2, y2};
    for (const auto &kv : options)
    {
        words.push_back("-" + kv.first);
        words.push_back(kv.second);
    }
    return call(words);
}

std::string Canvas::create_oval(const int& x1, const int& y1, const int& x2, const int& y2, const std::map<std::string, ArgValue>& options)
{
    std::vector<ArgValue> words = {impl_->full_name, "create", "oval", x1, y1, x2, y2};
    for (const auto &kv : options)
    {
        words.push_back("-" + kv.first);
        words.push_back(kv.second);
    }
    return call(words);
}

std::string Canvas::create_rectangle(const int& x1, const int& y1, const int& x2, const int& y2, const std::map<std::string, ArgValue>& options)
{
    std::vector<ArgValue> words = {impl_->full_name, "create", "rectangle", x1, y1, x2, y2};
    for (const auto &kv : options)
    {
        words.push_back("-" + kv.first);
        words.push_back(kv.second);
    }
    return call(words);
}

std::string Canvas::create_text(const int& x, const int& y, const std::map<std::string, ArgValue>& options)
{
    std::vector<ArgValue> words = {impl_->full_name, "create", "text", x, y};
    for (const auto &kv : options)
    {
        words.push_back("-" + kv.first);
        words.push_back(kv.second);
    }
    return call(words);
}

std::string Canvas::create_polygon(const std::vector<int>& coords, const std::map<std::string, ArgValue>& options)
{
    std::vector<ArgValue> words = {impl_->full_name, "create", "polygon"};
    for (int c : coords)
        words.push_back(c);
    for (const auto& kv : options)
    {
        words.push_back("-" + kv.first);
        words.push_back(kv.second);
    }
    return call(words);
}

std::string Canvas::create_arc(int x1, int y1, int x2, int y2, const std::map<std::string, ArgValue>& options)
{
    std::vector<ArgValue> words = {impl_->full_name, "create", "arc", x1, y1, x2, y2};
    for (const auto& kv : options)
    {
        words.push_back("-" + kv.first);
        words.push_back(kv.second);
    }
    return call(words);
}

std::string Canvas::create_image(int x, int y, const std::map<std::string, ArgValue>& options)
{
    std::vector<ArgValue> words = {impl_->full_name, "create", "image", x, y};
    for (const auto& kv : options)
    {
        words.push_back("-" + kv.first);
        words.push_back(kv.second);
    }
    return call(words);
}

std::string Canvas::create_bitmap(int x, int y, const std::map<std::string, ArgValue>& options)
{
    std::vector<ArgValue> words = {impl_->full_name, "create", "bitmap", x, y};
    for (const auto& kv : options)
    {
        words.push_back("-" + kv.first);
        words.push_back(kv.second);
    }
    return call(words);
}

std::string Canvas::create_window(int x, int y, const Widget& widget, const std::map<std::string, ArgValue>& options)
{
    std::vector<ArgValue> words = {impl_->full_name, "create", "window", x, y, "-window", widget.full_name()};
    for (const auto& kv : options)
    {
        words.push_back("-" + kv.first);
        words.push_back(kv.second);
    }
    return call(words);
}

std::vector<std::string> Canvas::find_overlapping(int x1, int y1, int x2, int y2) const
{
    auto result = call({impl_->full_name, "find", "overlapping", x1, y1, x2, y2});
    return impl_->interp->split_list(result);
}

std::vector<std::string> Canvas::find_closest(int x, int y) const
{
    auto result = call({impl_->full_name, "find", "closest", x, y});
    return impl_->interp->split_list(result);
}

std::vector<std::string> Canvas::find_all() const
{
    auto result = call({impl_->full_name, "find", "all"});
    return impl_->interp->split_list(result);
}

std::vector<std::string> Canvas::find_withtag(const std::string& tag_or_id) const
{
    auto result = call({impl_->full_name, "find", "withtag", tag_or_id});
    return impl_->interp->split_list(result);
}

Canvas& Canvas::tag_raise(const std::string& id_or_tag, const std::string& above_this)
{
    std::vector<ArgValue> words = {impl_->full_name, "raise", id_or_tag};
    if (!above_this.empty())
        words.push_back(above_this);
    call(words);
    return *this;
}

Canvas& Canvas::tag_lower(const std::string& id_or_tag, const std::string& below_this)
{
    std::vector<ArgValue> words = {impl_->full_name, "lower", id_or_tag};
    if (!below_this.empty())
        words.push_back(below_this);
    call(words);
    return *this;
}

double Canvas::canvasx(int screen_x) const
{
    auto ret = call({impl_->full_name, "canvasx", screen_x});
    return safe_stod(ret.c_str());
}

double Canvas::canvasy(int screen_y) const
{
    auto ret = call({impl_->full_name, "canvasy", screen_y});
    return safe_stod(ret.c_str());
}

Canvas& Canvas::addtag(const std::string& tag, const std::string& where, const std::string& target)
{
    call({impl_->full_name, "addtag", tag, where, target});
    return *this;
}

Canvas& Canvas::dtag(const std::string& tag, const std::string& target)
{
    call({impl_->full_name, "dtag", target, tag});
    return *this;
}

std::vector<std::string> Canvas::gettags(const std::string& id) const
{
    auto result = call({impl_->full_name, "gettags", id});
    return impl_->interp->split_list(result);
}

Canvas& Canvas::coords(const std::string& item_id, const std::vector<int>& coords)
{
    std::vector<ArgValue> words = {impl_->full_name, "coords", item_id};
    for (int c : coords)
        words.push_back(c);
    call(words);
    return *this;
}

Canvas& Canvas::move(const std::string& id_or_tag, const int& x, const int& y)
{
    call({impl_->full_name, "move", id_or_tag, x, y});
    return *this;
}

Canvas& Canvas::moveto(const std::string& id_or_tag, const int& x, const int& y)
{
    call({impl_->full_name, "moveto", id_or_tag, x, y});
    return *this;
}

Canvas& Canvas::xview(const std::string& args)
{
    std::vector<ArgValue> words = {impl_->full_name, "xview"};
    for (const auto& token : impl_->interp->split_list(args))
        words.emplace_back(token);
    call(words);
    return *this;
}

Canvas& Canvas::yview(const std::string& args)
{
    std::vector<ArgValue> words = {impl_->full_name, "yview"};
    for (const auto& token : impl_->interp->split_list(args))
        words.emplace_back(token);
    call(words);
    return *this;
}

Canvas& Canvas::xscrollcommand(std::function<void(std::string)> callback)
{
    auto cb_name = sanitize(impl_->full_name) + "_xstring_cb";
    register_string_callback(cb_name, callback);
    config({{"xscrollcommand", cb_name}});
    return *this;
}

Canvas& Canvas::yscrollcommand(std::function<void(std::string)> callback)
{
    auto cb_name = sanitize(impl_->full_name) + "_ystring_cb";
    register_string_callback(cb_name, callback);
    config({{"yscrollcommand", cb_name}});
    return *this;
}

Canvas& Canvas::scale(const std::string& id_or_tag, const int& x, const int& y, const double& xscale, const double& yscale)
{
    call({impl_->full_name, "scale", id_or_tag, x, y, xscale, yscale});
    return *this;
}

Canvas& Canvas::erase(const std::string& id_or_tag)
{
    call({impl_->full_name, "delete", id_or_tag});
    return *this;
}

Canvas& Canvas::width(const int &width)
{
    config({{"width", std::to_string(width)}});
    return *this;
}

Canvas& Canvas::height(const int &height)
{
    config({{"height", std::to_string(height)}});
    return *this;
}

std::string Canvas::postscript(const std::map<std::string, ArgValue>& options)
{
    std::vector<ArgValue> words = {impl_->full_name, "postscript"};
    for (const auto& kv : options)
    {
        words.push_back("-" + kv.first);
        words.push_back(kv.second);
    }
    return call(words);
}

Entry::Entry(const Widget& parent, const std::map<std::string, ArgValue>& options)
    : Widget(parent, "entry", "e", options)
{
}

Entry& Entry::textvariable(StringVar& var)
{
    config({{"textvariable", var}});
    return *this;
}

Entry& Entry::state(const std::string& state)
{
    config({{"state", state}});
    return *this;
}

Entry& Entry::icursor(const std::string& index)
{
    call({impl_->full_name, "icursor", index});
    return *this;
}

Entry& Entry::insert(const std::string& index, const std::string& text)
{
    call({impl_->full_name, "insert", index, text});
    return *this;
}

int Entry::index(const std::string& index) const
{
    auto ok  = false;
    auto ret = call({impl_->full_name, "index", index}, &ok);
    if (!ok)
        return -1;
    return std::stol(ret);
}

Entry& Entry::erase(const std::string& start, const std::string& end)
{
    std::vector<ArgValue> words = {impl_->full_name, "delete", start};
    if (!end.empty())
        words.push_back(end);
    call(words);
    return *this;
}

std::string Entry::get() const
{
    auto ok  = false;
    auto ret = call({impl_->full_name, "get"}, &ok);
    return ret;
}

Entry& Entry::xscrollcommand(std::function<void(std::string)> callback)
{
    auto cb_name = sanitize(full_name()) + "_xstring_cb";
    register_string_callback(cb_name, callback);
    config({{"xscrollcommand", cb_name}});
    return *this;
}

Entry& Entry::xview(const std::string& args)
{
    std::vector<ArgValue> words = {impl_->full_name, "xview"};
    for (const auto& token : impl_->interp->split_list(args))
        words.emplace_back(token);
    call(words);
    return *this;
}

Entry& Entry::select_range(const std::string& start, const std::string& end)
{
    call({impl_->full_name, "selection", "range", start, end});
    return *this;
}

Entry& Entry::selection_clear()
{
    call({impl_->full_name, "selection", "clear"});
    return *this;
}

bool Entry::select_present() const
{
    auto ret = call({impl_->full_name, "selection", "present"});
    return safe_stol(ret.c_str()) != 0;
}

Entry& Entry::validate(const std::string& mode, std::function<bool(const std::string&)> callback)
{
    auto cb_name = sanitize(full_name()) + "_validate_cb";
    register_bool_callback(cb_name, callback);
    config({{"validate", mode}, {"validatecommand", cb_name + " %P"}});
    return *this;
}

Label::Label(const Widget& parent, const std::map<std::string, ArgValue>& options)
    : Widget(parent, "label", "l", options)
{
}

Label& Label::text(const std::string &text)
{
    config({{"text", text}});
    return *this;
}

LabelFrame::LabelFrame(const Widget& parent, const std::map<std::string, ArgValue>& options)
    : Widget(parent, "labelframe", "labelframe", options)
{
}

LabelFrame& LabelFrame::width(const int& width)
{
    config({{"width", std::to_string(width)}});
    return *this;
}

LabelFrame& LabelFrame::height(const int& height)
{
    config({{"height", std::to_string(height)}});
    return *this;
}

LabelFrame& LabelFrame::text(const std::string& text)
{
    config({{"text", text}});
    return *this;
}

Listbox::Listbox(const Widget& parent, const std::map<std::string, ArgValue>& options)
    : Widget(parent, "listbox", "listbox", options)
{
}

Listbox& Listbox::insert(const std::string& index, const std::string& item)
{
    call({impl_->full_name, "insert", index, item});
    return *this;
}

Listbox& Listbox::erase(const std::string& start, const std::string& end)
{
    std::vector<ArgValue> words = {impl_->full_name, "delete", start};
    if (!end.empty())
        words.push_back(end);
    call(words);
    return *this;
}

std::vector<int> Listbox::curselection() const
{
    std::string result = call({impl_->full_name, "curselection"});
    std::vector<int> indices;
    for (const auto& token : impl_->interp->split_list(result))
        indices.push_back(safe_stol(token.c_str()));
    return indices;
}

std::string Listbox::get(const std::string& index) const
{
    return call({impl_->full_name, "get", index});
}

Listbox& Listbox::yscrollcommand(std::function<void(std::string)> callback)
{
    auto cb_name = sanitize(full_name()) + "_ystring_cb";
    register_string_callback(cb_name, callback);
    config({{"yscrollcommand", cb_name}});
    return *this;
}

Listbox& Listbox::yview(const std::string& args)
{
    std::vector<ArgValue> words = {impl_->full_name, "yview"};
    for (const auto& token : impl_->interp->split_list(args))
        words.emplace_back(token);
    call(words);
    return *this;
}

Listbox& Listbox::xscrollcommand(std::function<void(std::string)> callback)
{
    auto cb_name = sanitize(full_name()) + "_xstring_cb";
    register_string_callback(cb_name, callback);
    config({{"xscrollcommand", cb_name}});
    return *this;
}

Listbox& Listbox::xview(const std::string& args)
{
    std::vector<ArgValue> words = {impl_->full_name, "xview"};
    for (const auto& token : impl_->interp->split_list(args))
        words.emplace_back(token);
    call(words);
    return *this;
}

Listbox& Listbox::selectmode(const std::string& mode)
{
    config({{"selectmode", mode}}); // "single", "browse", "multiple", "extended"
    return *this;
}

Listbox& Listbox::see(const std::string& index)
{
    call({impl_->full_name, "see", index});
    return *this;
}

int Listbox::nearest(int y) const
{
    auto ret = call({impl_->full_name, "nearest", y});
    return safe_stol(ret.c_str());
}

int Listbox::size() const
{
    auto ret = call({impl_->full_name, "size"});
    return safe_stol(ret.c_str());
}

Listbox& Listbox::select_set(const std::string& first, const std::string& last)
{
    std::vector<ArgValue> words = {impl_->full_name, "selection", "set", first};
    if (!last.empty())
        words.push_back(last);
    call(words);
    return *this;
}

Listbox& Listbox::select_clear(const std::string& first, const std::string& last)
{
    std::vector<ArgValue> words = {impl_->full_name, "selection", "clear", first};
    if (!last.empty())
        words.push_back(last);
    call(words);
    return *this;
}

bool Listbox::select_includes(const std::string& index) const
{
    auto ret = call({impl_->full_name, "selection", "includes", index});
    return safe_stol(ret.c_str()) != 0;
}

Listbox& Listbox::activate(const std::string& index)
{
    call({impl_->full_name, "activate", index});
    return *this;
}

Menu::Menu(const Widget& parent, const std::map<std::string, ArgValue>& options)
    : Widget(parent, "menu", "menu", options)
{
}

Menu& Menu::add_command(const std::map<std::string, ArgValue>& options, std::function<void()> callback)
{
    std::vector<ArgValue> words = {impl_->full_name, "add", "command"};
    for (const auto& kv : options)
    {
        words.push_back("-" + kv.first);
        words.push_back(kv.second);
    }
    if (callback)
    {
        auto cb_name = next_menu_entry_callback_name(impl_->full_name);
        register_void_callback(cb_name, callback);
        words.push_back("-command");
        words.push_back(cb_name);
    }
    call(words);
    return *this;
}

Menu& Menu::add_cascade(const std::map<std::string, ArgValue>& options)
{
    std::vector<ArgValue> words = {impl_->full_name, "add", "cascade"};
    for (const auto& kv : options)
    {
        words.push_back("-" + kv.first);
        words.push_back(kv.second);
    }
    call(words);
    return *this;
}

Menu& Menu::add_separator()
{
    call({impl_->full_name, "add", "separator"});
    return *this;
}

Menu& Menu::add_checkbutton(const std::map<std::string, ArgValue>& options, std::function<void()> callback)
{
    std::vector<ArgValue> words = {impl_->full_name, "add", "checkbutton"};
    for (const auto& kv : options)
    {
        words.push_back("-" + kv.first);
        words.push_back(kv.second);
    }
    if (callback)
    {
        auto cb_name = next_menu_entry_callback_name(impl_->full_name);
        register_void_callback(cb_name, callback);
        words.push_back("-command");
        words.push_back(cb_name);
    }
    call(words);
    return *this;
}

Menu& Menu::add_radiobutton(const std::map<std::string, ArgValue>& options, std::function<void()> callback)
{
    std::vector<ArgValue> words = {impl_->full_name, "add", "radiobutton"};
    for (const auto& kv : options)
    {
        words.push_back("-" + kv.first);
        words.push_back(kv.second);
    }
    if (callback)
    {
        auto cb_name = next_menu_entry_callback_name(impl_->full_name);
        register_void_callback(cb_name, callback);
        words.push_back("-command");
        words.push_back(cb_name);
    }
    call(words);
    return *this;
}

Menu& Menu::insert(const std::string& index, const std::string& item_type, const std::map<std::string, ArgValue>& options, std::function<void()> callback)
{
    std::vector<ArgValue> words = {impl_->full_name, "insert", index, item_type};
    for (const auto& kv : options)
    {
        words.push_back("-" + kv.first);
        words.push_back(kv.second);
    }
    if (callback)
    {
        auto cb_name = next_menu_entry_callback_name(impl_->full_name);
        register_void_callback(cb_name, callback);
        words.push_back("-command");
        words.push_back(cb_name);
    }
    call(words);
    return *this;
}

Menu& Menu::entryconfigure(const std::string& index, const std::map<std::string, ArgValue>& options, std::function<void()> callback)
{
    std::vector<ArgValue> words = {impl_->full_name, "entryconfigure", index};
    for (const auto& kv : options)
    {
        words.push_back("-" + kv.first);
        words.push_back(kv.second);
    }
    if (callback)
    {
        auto cb_name = next_menu_entry_callback_name(impl_->full_name);
        register_void_callback(cb_name, callback);
        words.push_back("-command");
        words.push_back(cb_name);
    }
    call(words);
    return *this;
}

int Menu::index(const std::string& pattern) const
{
    // patternに一致する項目が無い場合、Tclは"none"を返すのではなくエラーにする
    // ("bad menu entry index")。successを明示的に見て、失敗時もErrorPolicyに関わらず
    // 例外を投げず-1を返す(本メソッドの「見つからなければ-1」という契約を守るため)。
    bool ok = false;
    auto ret = call({impl_->full_name, "index", pattern}, &ok);
    if (!ok || ret == "none")
        return -1;
    return safe_stol(ret.c_str());
}

Menu& Menu::erase(const std::string& index)
{
    call({impl_->full_name, "delete", index});
    return *this;
}

Menu& Menu::post(int x, int y)
{
    call({impl_->full_name, "post", x, y});
    return *this;
}

Menu& Menu::unpost()
{
    call({impl_->full_name, "unpost"});
    return *this;
}

Menubutton::Menubutton(const Widget& parent)
    : Widget(parent, "menubutton", "mb")
{}

Menubutton& Menubutton::menu(Menu* menu)
{
    config({{"menu", menu->full_name()}});
    return *this;
}

OptionMenu::OptionMenu(const Widget& parent, StringVar& variable, const std::vector<std::string>& values)
    : Widget(parent, "menubutton", "optmenu")
    , command_(std::make_shared<std::function<void(const std::string&)>>())
{
    menu_ = Menu(*this);
    for (std::size_t i = 0; i < values.size(); ++i)
    {
        std::string value = values[i];
        auto cb_name = sanitize(full_name()) + "_optmenu_cb_" + std::to_string(i);
        auto* var_ptr = &variable;
        auto cmd = command_;
        register_void_callback(cb_name, [var_ptr, value, cmd]() {
            var_ptr->set(value);
            if (*cmd) (*cmd)(value);
        });
        menu_.add_command({{"label", value}, {"command", cb_name}});
    }
    config({{"menu", menu_.full_name()}, {"textvariable", variable.name()}});
    if (!values.empty())
        variable.set(values.front());
}

OptionMenu& OptionMenu::command(std::function<void(const std::string&)> callback)
{
    *command_ = callback;
    return *this;
}

Message::Message(const Widget& parent)
    : Widget(parent, "message", "msg")
{}

Message& Message::text(const std::string& text)
{
    config({{"text", text}});
    return *this;
}

PanedWindow::PanedWindow(const Widget& parent)
    : Widget(parent, "panedwindow", "pw")
{}

PanedWindow& PanedWindow::orient(const std::string& dir)
{
    config({{"orient", dir}});
    return *this;
}

PanedWindow& PanedWindow::add(const Widget& child, const std::map<std::string, ArgValue>& options)
{
    std::vector<ArgValue> words = {impl_->full_name, "add", child.full_name()};
    for (const auto& kv : options)
    {
        words.push_back("-" + kv.first);
        words.push_back(kv.second);
    }
    call(words);
    return *this;
}

PanedWindow& PanedWindow::forget(const Widget& child)
{
    call({impl_->full_name, "forget", child.full_name()});
    return *this;
}

Radiobutton::Radiobutton(const Widget& parent, const std::map<std::string, ArgValue>& options)
    : Widget(parent, "radiobutton", "rb", options)
{
}

Radiobutton& Radiobutton::text(const std::string& text)
{
    config({{"text", text}});
    return *this;
}

Radiobutton& Radiobutton::variable(Var& var)
{
    config({{"variable", var}});
    return *this;
}

Radiobutton& Radiobutton::value(const std::string& val)
{
    call({impl_->full_name, "configure", "-value", val});
    return *this;
}

Radiobutton& Radiobutton::command(std::function<void()> callback)
{
    auto cb = sanitize(full_name()) + "_rb_cb";
    register_void_callback(cb, callback);
    config({{"command", cb}});
    return *this;
}

std::string Radiobutton::invoke()
{
    return Widget::call({impl_->full_name, "invoke"});
}

Scale::Scale(const Widget& parent, const std::map<std::string, ArgValue>& options)
    : Widget(parent, "scale", "scale", options)
{
}
    
Scale& Scale::from(double val) 
{ 
    config({{"from", std::to_string(val)}}); 
    return *this; 
}

Scale& Scale::to(double val)
{ 
    config({{"to", std::to_string(val)}}); 
    return *this; 
}

Scale& Scale::orient(const std::string& dir) 
{ 
    config({{"orient", dir}}); 
    return *this; 
}

Scale& Scale::command(std::function<void(const double&)> callback)
{
    std::string callback_name = sanitize(full_name()) + "_double_cb";
    register_double_callback(callback_name, callback);
    config({{"command", callback_name}});
    return *this;
}

double Scale::get() const
{
    auto ret = call({impl_->full_name, "get"});
    return safe_stod(ret.c_str());
}

Scale& Scale::set(double value)
{
    call({impl_->full_name, "set", value});
    return *this;
}

Scrollbar::Scrollbar(const Widget& parent, const std::map<std::string, ArgValue>& options) 
    : Widget(parent, "scrollbar", "scrollbar", options)
{
}

Scrollbar& Scrollbar::orient(const std::string& dir) 
{
    config({{"orient", dir}});
    return *this;
}

Scrollbar& Scrollbar::command(std::function<void(const std::string&)> callback) 
{
    std::string callback_name = sanitize(full_name()) + "_scroll_cb";
    register_string_callback(callback_name, callback);
    config({{"command", callback_name}});
    return *this;
}

Scrollbar& Scrollbar::set(const std::string& args) 
{
    std::vector<ArgValue> words = {impl_->full_name, "set"};
    for (const auto& token : impl_->interp->split_list(args))
        words.emplace_back(token);
    call(words);
    return *this;
}

Spinbox::Spinbox(const Widget& parent, const std::map<std::string, ArgValue>& options)
    : Widget(parent, "spinbox", "spinbox", options)
{
}

Spinbox& Spinbox::from(double val)
{
    config({{"from", val}});
    return *this;
}

Spinbox& Spinbox::to(double val)
{
    config({{"to", val}});
    return *this;
}

Spinbox& Spinbox::increment(double val)
{
    config({{"increment", val}});
    return *this;
}

std::string Spinbox::get() const
{
    return call({impl_->full_name, "get"});
}

Spinbox& Spinbox::textvariable(StringVar& var)
{
    config({{"textvariable", var}});
    return *this;
}

Spinbox& Spinbox::command(std::function<void()> callback)
{
    auto cb = sanitize(full_name()) + "_sp_cb";
    register_void_callback(cb, callback);
    config({{"command", cb}});
    return *this;
}

Text::Text(const Widget& parent, const std::map<std::string, ArgValue>& options) 
    : Widget(parent, "text", "text", options)
{
}

Text& Text::insert(const std::string& index, const std::string& text) 
{
    call({impl_->full_name, "insert", index, text});
    return *this;
}

std::string Text::get(const std::string& start, const std::string& end) const 
{
    return call({impl_->full_name, "get", start, end});
}

Text& Text::erase(const std::string& start, const std::string& end) 
{
    call({impl_->full_name, "delete", start, end});
    return *this;
}

Text& Text::yscrollcommand(std::function<void(std::string)> callback) 
{
    auto cb_name = sanitize(full_name()) + "_string_cb";
    register_string_callback(cb_name, callback);        
    config({{"yscrollcommand", cb_name}});
    return *this;
}

Text& Text::yview(const std::string& args)
{
    std::vector<ArgValue> words = {impl_->full_name, "yview"};
    for (const auto& token : impl_->interp->split_list(args))
        words.emplace_back(token);
    call(words);
    return *this;
}

Text& Text::xscrollcommand(std::function<void(std::string)> callback)
{
    auto cb_name = sanitize(full_name()) + "_xstring_cb";
    register_string_callback(cb_name, callback);
    config({{"xscrollcommand", cb_name}});
    return *this;
}

Text& Text::xview(const std::string& args)
{
    std::vector<ArgValue> words = {impl_->full_name, "xview"};
    for (const auto& token : impl_->interp->split_list(args))
        words.emplace_back(token);
    call(words);
    return *this;
}

std::vector<int> Text::bbox(const std::string& index) const
{
    auto ok     = false;
    auto result = call({impl_->full_name, "bbox", index}, &ok);

    std::vector<int> box;
    if (!ok)
        return box;

    for (const auto& token : impl_->interp->split_list(result))
        box.push_back(safe_stol(token.c_str()));
    return box;
}

Text& Text::wrap(const std::string& mode)
{
    config({{"wrap", mode}});
    return *this;
}

Text& Text::tag_add(const std::string& tag, const std::string& start, const std::string& end)
{
    call({impl_->full_name, "tag", "add", tag, start, end});
    return *this;
}

Text& Text::tag_remove(const std::string& tag, const std::string& start, const std::string& end)
{
    call({impl_->full_name, "tag", "remove", tag, start, end});
    return *this;
}

Text& Text::tag_config(const std::string& tag, const std::map<std::string, ArgValue>& options)
{
    std::vector<ArgValue> words = {impl_->full_name, "tag", "configure", tag};
    for (const auto& kv : options)
    {
        words.push_back("-" + kv.first);
        words.push_back(kv.second);
    }
    call(words);
    return *this;
}

Text& Text::mark_set(const std::string& mark, const std::string& index)
{
    call({impl_->full_name, "mark", "set", mark, index});
    return *this;
}

Text& Text::mark_unset(const std::string& mark)
{
    call({impl_->full_name, "mark", "unset", mark});
    return *this;
}

std::string Text::search(const std::string& pattern, const std::string& index, const std::map<std::string, ArgValue>& options)
{
    std::vector<ArgValue> words = {impl_->full_name, "search"};
    for (const auto& kv : options)
    {
        words.push_back("-" + kv.first);
        words.push_back(kv.second);
    }
    words.push_back(pattern);
    words.push_back(index);
    auto ok  = false;
    auto ret = call(words, &ok);
    return ok ? ret : "";
}

Text& Text::see(const std::string& index)
{
    call({impl_->full_name, "see", index});
    return *this;
}

std::string Text::index(const std::string& index) const
{
    return call({impl_->full_name, "index", index});
}

std::vector<std::string> Text::tag_names(const std::string& index) const
{
    std::vector<ArgValue> words = {impl_->full_name, "tag", "names"};
    if (!index.empty())
        words.push_back(index);
    auto ret = call(words);
    return impl_->interp->split_list(ret);
}

std::vector<std::string> Text::tag_ranges(const std::string& tag) const
{
    auto ret = call({impl_->full_name, "tag", "ranges", tag});
    return impl_->interp->split_list(ret);
}

Text& Text::tag_raise(const std::string& tag, const std::string& above_this)
{
    std::vector<ArgValue> words = {impl_->full_name, "tag", "raise", tag};
    if (!above_this.empty())
        words.push_back(above_this);
    call(words);
    return *this;
}

Text& Text::tag_lower(const std::string& tag, const std::string& below_this)
{
    std::vector<ArgValue> words = {impl_->full_name, "tag", "lower", tag};
    if (!below_this.empty())
        words.push_back(below_this);
    call(words);
    return *this;
}

Text& Text::tag_delete(const std::string& tag)
{
    call({impl_->full_name, "tag", "delete", tag});
    return *this;
}

Text& Text::edit_undo()
{
    bool ok = false;
    call({impl_->full_name, "edit", "undo"}, &ok); // undoスタックが空/無効な場合Tclがエラーにするため無視する
    return *this;
}

Text& Text::edit_redo()
{
    bool ok = false;
    call({impl_->full_name, "edit", "redo"}, &ok);
    return *this;
}

bool Text::edit_modified() const
{
    auto ret = call({impl_->full_name, "edit", "modified"});
    return safe_stol(ret.c_str()) != 0;
}

Text& Text::edit_modified(bool value)
{
    call({impl_->full_name, "edit", "modified", value});
    return *this;
}

bool Text::compare(const std::string& index1, const std::string& op, const std::string& index2) const
{
    auto ret = call({impl_->full_name, "compare", index1, op, index2});
    return safe_stol(ret.c_str()) != 0;
}

int Text::count(const std::string& index1, const std::string& index2, const std::string& option) const
{
    auto ret = call({impl_->full_name, "count", "-" + option, index1, index2});
    return safe_stol(ret.c_str());
}

std::string Text::dump(const std::string& index1, const std::string& index2) const
{
    std::vector<ArgValue> words = {impl_->full_name, "dump", index1};
    if (!index2.empty())
        words.push_back(index2);
    return call(words);
}

std::string Text::image_create(const std::string& index, const std::map<std::string, ArgValue>& options)
{
    std::vector<ArgValue> words = {impl_->full_name, "image", "create", index};
    for (const auto& kv : options)
    {
        words.push_back("-" + kv.first);
        words.push_back(kv.second);
    }
    return call(words);
}

Text& Text::window_create(const std::string& index, const Widget& window, const std::map<std::string, ArgValue>& options)
{
    std::vector<ArgValue> words = {impl_->full_name, "window", "create", index, "-window", window.full_name()};
    for (const auto& kv : options)
    {
        words.push_back("-" + kv.first);
        words.push_back(kv.second);
    }
    call(words);
    return *this;
}

namespace font
{

Font::Font(const std::map<std::string, ArgValue>& option, const std::string& name, bool exists)
    : interp_(current_interp())
{
    name_ = name.empty() ? ("font_" + id) : name;
    if (!exists)
    {
        call({"font", "create", name_});
    }
    if (!option.empty())
    {
        config(option);
    }
}

Font nametofont(const std::string& name)
{
    return Font({}, name, /*exists=*/true);
}

Font& Font::config(const std::map<std::string, ArgValue>& option)
{
    std::vector<ArgValue> words = {"font", "configure", name_};
    for (const auto &kv : option)
    {
        words.push_back("-" + kv.first);
        words.push_back(kv.second);
    }
    call(words);
    return *this;
}

Font& Font::size(const int& size)
{
    return config({{"size", size}});
}

Font& Font::weight(const std::string& weight)
{
    return config({{"weight", weight}});
}

Font& Font::family(const std::string& family)
{
    return config({{"family", family}});
}

Font& Font::slant(const std::string& slant)
{
    return config({{"slant", slant}});
}

Font& Font::underline(const int& underline)
{
    return config({{"underline", underline}});
}

Font& Font::overstrike(const int& overstrike)
{
    return config({{"overstrike", overstrike}});
}

std::string Font::actual(const std::string& option) const
{
    return call({"font", "actual", name_, "-" + option});
}

std::string Font::metrics(const std::string& option) const
{
    return call({"font", "metrics", name_, "-" + option});
}

Font Font::copy() const
{
    // デフォルト引数のFont()はcurrent_interp()に束縛された(既定属性の)フォントを既に生成済み
    // のため、"font create"の再呼び出しではなく"font configure"で属性を上書きする。
    Font result;

    auto spec   = call({"font", "actual", name_});
    auto tokens = interp_->split_list(spec);

    std::vector<ArgValue> words = {"font", "configure", result.name_};
    for (const auto& token : tokens)
        words.emplace_back(token);
    call(words);
    return result;
}

std::vector<std::string> families()
{
    auto* interp = current_interp();
    bool ok = false;
    auto ret = interp->call({"font", "families"}, &ok);
    if (!ok) return {};
    return interp->split_list(ret);
}

std::vector<std::string> names()
{
    auto* interp = current_interp();
    bool ok = false;
    auto ret = interp->call({"font", "names"}, &ok);
    if (!ok) return {};
    return interp->split_list(ret);
}

int Font::measure(const std::string& text) const
{
    auto ret = call({"font", "measure", name_, text});
    return safe_stol(ret.c_str());
}

const std::string& Font::name() const
{
    return name_;
}

} // font

namespace ttk
{

Style::Style()
    : interp_(current_interp())
{}

Style& Style::configure(const std::string& style_name, const std::map<std::string, ArgValue>& options)
{
    std::vector<ArgValue> words = {"ttk::style", "configure", style_name};
    for (const auto& kv : options)
    {
        words.push_back("-" + kv.first);
        words.push_back(kv.second);
    }
    call(words);
    return *this;
}

Style& Style::map(const std::string& style_name, const std::map<std::string, std::vector<std::string>>& options)
{
    std::vector<ArgValue> words = {"ttk::style", "map", style_name};
    for (const auto& kv : options)
    {
        words.push_back("-" + kv.first);
        words.push_back(std::vector<ArgValue>(kv.second.begin(), kv.second.end()));
    }
    call(words);
    return *this;
}

std::string Style::lookup(const std::string& style_name, const std::string& option) const
{
    return call({"ttk::style", "lookup", style_name, "-" + option});
}

std::vector<std::string> Style::theme_names() const
{
    return interp_->split_list(call({"ttk::style", "theme", "names"}));
}

Style& Style::theme_use(const std::string& theme_name)
{
    // Python本家もttk::styleではなくttk::setThemeを使う($ttk::currentThemeの追従のため)
    call({"ttk::setTheme", theme_name});
    return *this;
}

std::string Style::theme_use() const
{
    return call({"ttk::style", "theme", "use"});
}

std::string Style::layout(const std::string& style_name) const
{
    return call({"ttk::style", "layout", style_name});
}

Style& Style::layout(const std::string& style_name, const std::string& layout_spec)
{
    call({"ttk::style", "layout", style_name, layout_spec});
    return *this;
}

Style& Style::element_create(const std::string& name, const std::string& type, const std::vector<ArgValue>& args)
{
    std::vector<ArgValue> words = {"ttk::style", "element", "create", name, type};
    for (const auto& arg : args)
        words.push_back(arg);
    call(words);
    return *this;
}

Style& Style::theme_create(const std::string& name, const std::string& parent, const std::string& settings_script)
{
    std::vector<ArgValue> words = {"ttk::style", "theme", "create", name};
    if (!parent.empty())
    {
        words.push_back("-parent");
        words.push_back(parent);
    }
    if (!settings_script.empty())
    {
        words.push_back("-settings");
        words.push_back(settings_script);
    }
    call(words);
    return *this;
}

Button::Button(const Widget& parent, const std::map<std::string, ArgValue>& options)
    : Widget(parent, "ttk::button", "ttk_b", options)
{
}

Button& Button::width(const int& width)
{
    config({{"width",  std::to_string(width)}});
    return *this;
}

Button& Button::height(const int& height)
{
    config({{"height",  std::to_string(height)}});
    return *this;
}

Button& Button::text(const std::string& text)
{
    config({{"text", text}});        
    return *this;
}

Button& Button::command(std::function<void()> callback)
{
    std::string callback_name =  sanitize(full_name()) + "_void_callback";
    register_void_callback(callback_name, callback);
    config({{"command", callback_name}});        
    return *this;
}

Button& Button::font(const font::Font& font)
{
    config({{"font", font.name()}});
    return *this;
}

std::string Button::invoke()
{
    return Widget::call({impl_->full_name, "invoke"});
}

Checkbutton::Checkbutton(const Widget& parent, const std::map<std::string, ArgValue>& options)
    : Widget(parent, "ttk::checkbutton", "ttk_checkbutton", options)
{
}

Checkbutton& Checkbutton::text(const std::string& text)
{
    config({{"text", text}});
    return *this;
}

Checkbutton& Checkbutton::variable(Var& var)
{
    config({{"variable", var}});
    return *this;
}

Checkbutton& Checkbutton::command(std::function<void()> callback)
{
    auto cb = sanitize(full_name()) + "_chk_cb";
    register_void_callback(cb, callback);
    config({{"command", cb}});
    return *this;
}

std::string Checkbutton::invoke()
{
    return Widget::call({impl_->full_name, "invoke"});
}

Combobox::Combobox(const Widget& parent, const std::map<std::string, ArgValue>& options)
    : Widget(parent, "ttk::combobox", "ttk_combobox", options)
{
}

Combobox& Combobox::values(const std::vector<std::string>& items)
{
    // ArgValue(vector<ArgValue>)がTcl_NewListObj経由で要素ごとに正しくquoteするため、
    // 手組みの "{" + item + "}" 文字列連結(itemが"}"を含むと壊れる)は不要。
    config({{"values", std::vector<ArgValue>(items.begin(), items.end())}});
    return *this;
}

Combobox& Combobox::textvariable(StringVar& var)
{
    config({{"textvariable", var}});
    return *this;
}

Combobox& Combobox::width(const int& width)
{
    config({{"width", std::to_string(width)}});
    return *this;
}

Combobox& Combobox::height(const int& height)
{
    config({{"height", std::to_string(height)}});
    return *this;
}

Combobox& Combobox::justify(const std::string& justify)
{
    config({{"justify", justify}});
    return *this;
}

Combobox& Combobox::state(const std::string& state)
{
    config({{"state", state}});
    return *this;
}

Combobox& Combobox::font(const font::Font& font)
{
    config({{"font", font.name()}});
    return *this;
}

Combobox& Combobox::set(const std::string& value)
{
    call({impl_->full_name, "set", value});
    return *this;
}

Combobox& Combobox::insert(const std::string& index, const std::string& text) 
{
    call({impl_->full_name, "insert", index, text});
    return *this;
}

std::string Combobox::get() const 
{
    return call({impl_->full_name, "get"});
}

Combobox& Combobox::erase(const std::string& start, const std::string& end) 
{
    call({impl_->full_name, "delete", start, end});
    return *this;
}

int Combobox::current() const
{
    const auto val = call({impl_->full_name, "current"});
    return safe_stol(val.c_str());
}

Combobox& Combobox::current(const int& idx)
{
    call({impl_->full_name, "current", idx});
    return *this;    
}

Entry::Entry(const Widget& parent, const std::map<std::string, ArgValue>& options)
    : Widget(parent, "ttk::entry", "ttk_entry", options)
{
}

Entry& Entry::textvariable(StringVar& var)
{
    config({{"textvariable", var}});
    return *this;
}

Entry& Entry::state(const std::string& state)
{
    config({{"state", state}});
    return *this;
}

Entry& Entry::icursor(const std::string& index)
{
    call({impl_->full_name, "icursor", index});
    return *this;
}

Entry& Entry::insert(const std::string& index, const std::string& text) 
{
    call({impl_->full_name, "insert", index, text});
    return *this;
}

int Entry::index(const std::string& index) const 
{
    auto ok  = false;
    auto ret = call({impl_->full_name, "index", index}, &ok);
    if (!ok)
        return -1;
    return std::stol(ret);
}

Entry& Entry::erase(const std::string& start, const std::string& end) 
{
    std::vector<ArgValue> words = {impl_->full_name, "delete", start};
    if (!end.empty())
        words.push_back(end);
    call(words);
    return *this;
}

std::string Entry::get() const
{
    auto ok  = false;
    auto ret = call({impl_->full_name, "get"}, &ok);
    return ret;
}

Entry& Entry::xscrollcommand(std::function<void(std::string)> callback)
{
    auto cb_name = sanitize(full_name()) + "_xstring_cb";
    register_string_callback(cb_name, callback);
    config({{"xscrollcommand", cb_name}});
    return *this;
}

Entry& Entry::xview(const std::string& args)
{
    std::vector<ArgValue> words = {impl_->full_name, "xview"};
    for (const auto& token : impl_->interp->split_list(args))
        words.emplace_back(token);
    call(words);
    return *this;
}

Entry& Entry::select_range(const std::string& start, const std::string& end)
{
    call({impl_->full_name, "selection", "range", start, end});
    return *this;
}

Entry& Entry::selection_clear()
{
    call({impl_->full_name, "selection", "clear"});
    return *this;
}

bool Entry::select_present() const
{
    auto ret = call({impl_->full_name, "selection", "present"});
    return safe_stol(ret.c_str()) != 0;
}

Entry& Entry::validate(const std::string& mode, std::function<bool(const std::string&)> callback)
{
    auto cb_name = sanitize(full_name()) + "_validate_cb";
    register_bool_callback(cb_name, callback);
    config({{"validate", mode}, {"validatecommand", cb_name + " %P"}});
    return *this;
}

Frame::Frame(const Widget& parent, const std::map<std::string, ArgValue>& options)
    : Widget(parent, "ttk::frame", "ttk_frame", options)
{
}

Frame& Frame::width(const int &width)
{
    config({{"width", std::to_string(width)}});
    return *this;
}

Frame& Frame::height(const int &height)
{
    config({{"height", std::to_string(height)}});
    return *this;
}


Label::Label(const Widget& parent, const std::map<std::string, ArgValue>& options)
    : Widget(parent, "ttk::label", "tl", options)
{
}

Label& Label::text(const std::string &text)
{
    config({{"text", text}});
    return *this;
}

Label& Label::anchor(const std::string& anchor)
{
    config({{"anchor", anchor}});
    return *this;
}

Label& Label::relief(const std::string& relief)
{
    config({{"relief", relief}});
    return *this;
}

Label& Label::font(const font::Font& font)
{
    config({{"font", font.name()}});
    return *this;
}

Labelframe::Labelframe(const Widget& parent, const std::map<std::string, ArgValue>& options)
    : Widget(parent, "ttk::labelframe", "ttk_labelframe", options)
{
}

Labelframe& Labelframe::text(const std::string& text)
{
    config({{"text", text}});
    return *this;
}

Menubutton::Menubutton(const Widget& parent, const std::map<std::string, ArgValue>& options)
    : Widget(parent, "ttk::menubutton", "ttk_menubutton", options)
{
}

Menubutton& Menubutton::menu(Menu* menu)
{
    config({{"menu", menu->full_name()}});
    return *this;
}

Notebook::Notebook(const Widget& parent, const std::map<std::string, ArgValue>& options)
    : Widget(parent, "ttk::notebook", "ttk_notebook", options)
{
}

Notebook& Notebook::add_tab(const Widget& child, const std::string& label) 
{
    call({impl_->full_name, "add", child.full_name(), "-text", label});
    return *this;
}

Notebook& Notebook::select(const std::string& tab_id)
{
    call({impl_->full_name, "select", tab_id});
    return *this;
}

std::string Notebook::select() const
{
    return call({impl_->full_name, "select"});
}

Notebook& Notebook::tab(const std::string& tab_id, const std::map<std::string, ArgValue>& options)
{
    std::vector<ArgValue> words = {impl_->full_name, "tab", tab_id};
    for (const auto& kv : options)
    {
        words.push_back("-" + kv.first);
        words.push_back(kv.second);
    }
    call(words);
    return *this;
}

std::string Notebook::tab(const std::string& tab_id, const std::string& option) const
{
    return call({impl_->full_name, "tab", tab_id, "-" + option});
}

std::vector<std::string> Notebook::tabs() const
{
    auto ret = call({impl_->full_name, "tabs"});
    return impl_->interp->split_list(ret);
}

Notebook& Notebook::forget(const std::string& tab_id)
{
    call({impl_->full_name, "forget", tab_id});
    return *this;
}

Notebook& Notebook::hide(const std::string& tab_id)
{
    call({impl_->full_name, "hide", tab_id});
    return *this;
}

int Notebook::index(const std::string& tab_id) const
{
    auto ret = call({impl_->full_name, "index", tab_id});
    return safe_stol(ret.c_str());
}

PanedWindow::PanedWindow(const Widget& parent, const std::map<std::string, ArgValue>& options)
    : Widget(parent, "ttk::panedwindow", "ttk_panedwindow", options)
{
}

PanedWindow& PanedWindow::orient(const std::string& dir)
{
    config({{"orient", dir}});
    return *this;
}

PanedWindow& PanedWindow::add(const Widget& child, const std::map<std::string, ArgValue>& options)
{
    std::vector<ArgValue> words = {impl_->full_name, "add", child.full_name()};
    for (const auto& kv : options)
    {
        words.push_back("-" + kv.first);
        words.push_back(kv.second);
    }
    call(words);
    return *this;
}

PanedWindow& PanedWindow::forget(const Widget& child)
{
    call({impl_->full_name, "forget", child.full_name()});
    return *this;
}

int PanedWindow::sashpos(int index) const
{
    auto ret = call({impl_->full_name, "sashpos", index});
    return safe_stol(ret.c_str());
}

PanedWindow& PanedWindow::sashpos(int index, int newpos)
{
    call({impl_->full_name, "sashpos", index, newpos});
    return *this;
}

std::vector<std::string> PanedWindow::panes() const
{
    auto ret = call({impl_->full_name, "panes"});
    return impl_->interp->split_list(ret);
}

Progressbar::Progressbar(const Widget& parent, const std::map<std::string, ArgValue>& options)
    : Widget(parent, "ttk::progressbar", "ttk_progress", options)
{
}

Progressbar& Progressbar::mode(const std::string& mode)
{
    config({{"mode", mode}});   // "determinate" or "indeterminate"
    return *this;
}

Progressbar& Progressbar::value(double v)
{
    config({{"value", v}});
    return *this;
}

Progressbar& Progressbar::start(int interval)
{
    call({impl_->full_name, "start", interval});
    return *this;
}

Progressbar& Progressbar::stop()
{
    call({impl_->full_name, "stop"});
    return *this;
}

Progressbar& Progressbar::step(double amount)
{
    call({impl_->full_name, "step", amount});
    return *this;
}

Radiobutton::Radiobutton(const Widget& parent, const std::map<std::string, ArgValue>& options)
    : Widget(parent, "ttk::radiobutton", "ttk_radiobutton", options)
{
}

Radiobutton& Radiobutton::text(const std::string& text)
{
    config({{"text", text}});
    return *this;
}

Radiobutton& Radiobutton::variable(Var& var)
{
    config({{"variable", var}});
    return *this;
}

Radiobutton& Radiobutton::value(const std::string& val)
{
    call({impl_->full_name, "configure", "-value", val});
    return *this;
}

Radiobutton& Radiobutton::command(std::function<void()> callback)
{
    auto cb = sanitize(full_name()) + "_rb_cb";
    register_void_callback(cb, callback);
    config({{"command", cb}});
    return *this;
}

std::string Radiobutton::invoke()
{
    return Widget::call({impl_->full_name, "invoke"});
}

Separator::Separator(const Widget& parent, const std::map<std::string, ArgValue>& options)
    : Widget(parent, "ttk::separator", "ttk_separator", options)
{}

Scale::Scale(const Widget& parent, const std::map<std::string, ArgValue>& options)
    : Widget(parent, "ttk::scale", "ttk_scale", options)
{
}
    
Scale& Scale::from(double val) 
{ 
    config({{"from", std::to_string(val)}}); 
    return *this; 
}

Scale& Scale::to(double val)
{ 
    config({{"to", std::to_string(val)}}); 
    return *this; 
}

Scale& Scale::orient(const std::string& dir) 
{ 
    config({{"orient", dir}}); 
    return *this; 
}

Scale& Scale::command(std::function<void(const double&)> callback)
{
    std::string callback_name = sanitize(full_name()) + "_double_cb";
    register_double_callback(callback_name, callback);
    config({{"command", callback_name}});
    return *this;
}

double Scale::get() const
{
    auto ret = call({impl_->full_name, "get"});
    return safe_stod(ret.c_str());
}

Scale& Scale::set(double value)
{
    call({impl_->full_name, "set", value});
    return *this;
}

Scrollbar::Scrollbar(const Widget& parent, const std::map<std::string, ArgValue>& options) 
    : Widget(parent, "ttk::scrollbar", "ttk_scrollbar", options)
{
}

Scrollbar& Scrollbar::orient(const std::string& dir) 
{
    config({{"orient", dir}});
    return *this;
}

Scrollbar& Scrollbar::command(std::function<void(const std::string&)> callback) 
{
    std::string callback_name = sanitize(full_name()) + "_scroll_cb";
    register_string_callback(callback_name, callback);
    config({{"command", callback_name}});
    return *this;
}

Scrollbar& Scrollbar::set(const std::string& args)
{
    std::vector<ArgValue> words = {impl_->full_name, "set"};
    for (const auto& token : impl_->interp->split_list(args))
        words.emplace_back(token);
    call(words);
    return *this;
}

Spinbox::Spinbox(const Widget& parent, const std::map<std::string, ArgValue>& options)
    : Widget(parent, "ttk::spinbox", "ttk_spinbox", options)
{
}

Spinbox& Spinbox::from(double val)
{
    config({{"from", val}});
    return *this;
}

Spinbox& Spinbox::to(double val)
{
    config({{"to", val}});
    return *this;
}

Spinbox& Spinbox::increment(double val)
{
    config({{"increment", val}});
    return *this;
}

std::string Spinbox::get() const
{
    return call({impl_->full_name, "get"});
}

Spinbox& Spinbox::textvariable(StringVar& var)
{
    config({{"textvariable", var}});
    return *this;
}

Spinbox& Spinbox::command(std::function<void()> callback)
{
    auto cb = sanitize(full_name()) + "_sp_cb";
    register_void_callback(cb, callback);
    config({{"command", cb}});
    return *this;
}

Sizegrip::Sizegrip(const Widget& parent, const std::map<std::string, ArgValue>& options)
    : Widget(parent, "ttk::sizegrip", "ttk_sizegrip", options)
{
}

Treeview::Treeview(const Widget& parent, const std::map<std::string, ArgValue>& options)
    : Widget(parent, "ttk::treeview", "tv", options)
{
}

Treeview& Treeview::insert(const std::string& parent, const std::string& index, const std::string& iid, const std::map<std::string, ArgValue>& options)
{
    std::vector<ArgValue> words = {impl_->full_name, "insert", parent, index, "-id", iid};
    for (const auto& kv : options)
    {
        words.push_back("-" + kv.first);
        words.push_back(kv.second);
    }
    call(words);
    return *this;
}

Treeview& Treeview::erase(const std::string& iid)
{
    call({impl_->full_name, "delete", iid});
    return *this;
}

Treeview& Treeview::item(const std::string& iid, const std::map<std::string, ArgValue>& options)
{
    if (options.empty())
    {
        // getter 的な使い方をしたい場合は、必要に応じて別メソッドを追加してもよい
        call({impl_->full_name, "item", iid});
        return *this;
    }

    std::vector<ArgValue> words = {impl_->full_name, "item", iid};
    for (const auto& kv : options)
    {
        words.push_back("-" + kv.first);
        words.push_back(kv.second);
    }
    call(words);
    return *this;
}

Treeview& Treeview::heading(const std::string& column, const std::map<std::string, ArgValue>& options, std::function<void()> callback)
{
    std::vector<ArgValue> words = {impl_->full_name, "heading", column};
    for (const auto& kv : options)
    {
        words.push_back("-" + kv.first);
        words.push_back(kv.second);
    }
    if (callback)
    {
        // 列名(column)はindexと違い挿入/削除で変動しない安定した識別子のため、
        // Menuの項目コールバックと異なりfull_name+columnから決定的に名前を生成できる。
        auto cb_name = sanitize(impl_->full_name) + "_heading_" + sanitize(column) + "_cb";
        register_void_callback(cb_name, callback);
        words.push_back("-command");
        words.push_back(cb_name);
    }
    call(words);
    return *this;
}

Treeview& Treeview::column(const std::string& column, const std::map<std::string, ArgValue>& options)
{
    std::vector<ArgValue> words = {impl_->full_name, "column", column};
    for (const auto& kv : options)
    {
        words.push_back("-" + kv.first);
        words.push_back(kv.second);
    }
    call(words);
    return *this;
}

std::vector<std::string> Treeview::selection() const
{
    auto ret = call({impl_->full_name, "selection"});
    return impl_->interp->split_list(ret);
}

// Tclの"ttk::treeview selection set/add/remove/toggle"はitemsを「1個のTclリスト引数」として
// 受け取る(iid毎に別々の位置引数として渡すものではない)。iidにスペースを含む場合、位置引数を
// 複数渡すとTclがその最初の引数だけを空白区切りのリストとして誤って再分割してしまうため、
// ArgValue(vector<ArgValue>)のLIST(Tcl_NewListObj経由で各要素を正しくquoteする)を使う。
Treeview& Treeview::selection_set(const std::vector<std::string>& iids)
{
    call({impl_->full_name, "selection", "set", std::vector<ArgValue>(iids.begin(), iids.end())});
    return *this;
}

Treeview& Treeview::selection_add(const std::vector<std::string>& iids)
{
    call({impl_->full_name, "selection", "add", std::vector<ArgValue>(iids.begin(), iids.end())});
    return *this;
}

Treeview& Treeview::selection_remove(const std::vector<std::string>& iids)
{
    call({impl_->full_name, "selection", "remove", std::vector<ArgValue>(iids.begin(), iids.end())});
    return *this;
}

Treeview& Treeview::selection_toggle(const std::vector<std::string>& iids)
{
    call({impl_->full_name, "selection", "toggle", std::vector<ArgValue>(iids.begin(), iids.end())});
    return *this;
}

bool Treeview::exists(const std::string& iid) const
{
    auto ret = call({impl_->full_name, "exists", iid});
    return safe_stol(ret.c_str()) != 0;
}

Treeview& Treeview::see(const std::string& iid)
{
    call({impl_->full_name, "see", iid});
    return *this;
}

Treeview& Treeview::xview(const std::string& args)
{
    std::vector<ArgValue> words = {impl_->full_name, "xview"};
    for (const auto& token : impl_->interp->split_list(args))
        words.emplace_back(token);
    call(words);
    return *this;
}

Treeview& Treeview::yview(const std::string& args)
{
    std::vector<ArgValue> words = {impl_->full_name, "yview"};
    for (const auto& token : impl_->interp->split_list(args))
        words.emplace_back(token);
    call(words);
    return *this;
}

Treeview& Treeview::xscrollcommand(std::function<void(std::string)> callback)
{
    auto cb_name = sanitize(impl_->full_name) + "_xstring_cb";
    register_string_callback(cb_name, callback);
    config({{"xscrollcommand", cb_name}});
    return *this;
}

Treeview& Treeview::yscrollcommand(std::function<void(std::string)> callback)
{
    auto cb_name = sanitize(impl_->full_name) + "_ystring_cb";
    register_string_callback(cb_name, callback);
    config({{"yscrollcommand", cb_name}});
    return *this;
}

Treeview& Treeview::set(const std::string& iid, const std::string& column, const ArgValue& value)
{
    call({impl_->full_name, "set", iid, column, value});
    return *this;
}

std::string Treeview::set(const std::string& iid, const std::string& column) const
{
    return call({impl_->full_name, "set", iid, column});
}

Treeview& Treeview::move(const std::string& iid, const std::string& parent, const std::string& index)
{
    call({impl_->full_name, "move", iid, parent, index});
    return *this;
}

Treeview& Treeview::detach(const std::string& iid)
{
    call({impl_->full_name, "detach", iid});
    return *this;
}

Treeview& Treeview::reattach(const std::string& iid, const std::string& parent, const std::string& index)
{
    call({impl_->full_name, "reattach", iid, parent, index});
    return *this;
}

std::vector<std::string> Treeview::get_children(const std::string& iid) const
{
    auto ret = call({impl_->full_name, "children", iid});
    return impl_->interp->split_list(ret);
}

std::string Treeview::parent(const std::string& iid) const
{
    return call({impl_->full_name, "parent", iid});
}

int Treeview::index(const std::string& iid) const
{
    auto ret = call({impl_->full_name, "index", iid});
    return std::stoi(ret);
}

Treeview& Treeview::focus(const std::string& iid)
{
    call({impl_->full_name, "focus", iid});
    return *this;
}

std::string Treeview::focus() const
{
    return call({impl_->full_name, "focus"});
}

Treeview& Treeview::tag_configure(const std::string& tag, const std::map<std::string, ArgValue>& options)
{
    std::vector<ArgValue> words = {impl_->full_name, "tag", "configure", tag};
    for (const auto& kv : options)
    {
        words.push_back("-" + kv.first);
        words.push_back(kv.second);
    }
    call(words);
    return *this;
}

Treeview& Treeview::tag_bind(const std::string& tag, const std::string& event, std::function<void(const Event&)> callback)
{
    auto cb_name = sanitize(full_name()) + "_tag_" + sanitize(tag) + "_" + sanitize(event);

    register_event_callback(cb_name, callback);

    std::string script = cb_name + " %x %y %X %Y %W %K %k %c %t %D";

    call({impl_->full_name, "tag", "bind", tag, event, script});
    return *this;
}

std::vector<std::string> Treeview::tag_has(const std::string& tag) const
{
    auto ret = call({impl_->full_name, "tag", "has", tag});
    return impl_->interp->split_list(ret);
}

std::string Treeview::identify_row(int y) const
{
    return call({impl_->full_name, "identify", "row", y});
}

std::string Treeview::identify_column(int x) const
{
    return call({impl_->full_name, "identify", "column", x});
}

std::vector<int> Treeview::bbox(const std::string& iid, const std::string& column) const
{
    std::vector<ArgValue> words = {impl_->full_name, "bbox", iid};
    if (!column.empty())
        words.push_back(column);

    bool ok = false;
    auto ret = call(words, &ok);
    std::vector<int> out;
    if (!ok)
        return out;
    for (const auto& token : impl_->interp->split_list(ret))
        out.push_back(safe_stol(token.c_str()));
    return out;
}

} // ttk

namespace colorchooser
{

std::string askcolor(const std::map<std::string, ArgValue>& options)
{
    auto* interp = cpp_tk::interp_map[std::this_thread::get_id()];
    if (!interp)
        return "";

    std::vector<ArgValue> words = {"tk_chooseColor"};
    for (const auto& kv : options)
    {
        words.push_back("-" + kv.first);
        words.push_back(kv.second);
    }

    bool ok = false;
    auto ret = interp->call(words, &ok);
    return ok ? ret : "";
}

} // namespace colorchooser

namespace filedialog
{

static std::string invoke_dialog(const std::string& cmd_name, const std::map<std::string, ArgValue>& options)
{
    auto* interp = interp_map[std::this_thread::get_id()];
    if (!interp) return "";
    std::vector<ArgValue> words = {cmd_name};
    for (const auto& kv : options)
    {
        words.push_back("-" + kv.first);
        words.push_back(kv.second);
    }
    return interp->call(words);
}

std::string askopenfile(const std::map<std::string, ArgValue>& options)
{
    return invoke_dialog("tk_getOpenFile", options);
}

std::vector<std::string> askopenfilenames(const std::map<std::string, ArgValue>& options)
{
    auto* interp = interp_map[std::this_thread::get_id()];
    if (!interp) return {};

    std::vector<ArgValue> words = {"tk_getOpenFile", "-multiple", 1};
    for (const auto& kv : options)
    {
        words.push_back("-" + kv.first);
        words.push_back(kv.second);
    }
    // 戻り値はTclのリスト形式(要素にスペースを含む場合は{}で囲まれる)のため、
    // 単純な空白splitではなくTcl_SplitListで正しく要素分解する。
    return interp->split_list(interp->call(words));
}

std::string asksaveasfilename(const std::map<std::string, ArgValue>& options)
{
    return invoke_dialog("tk_getSaveFile", options);
}

std::string askdirectory(const std::map<std::string, ArgValue>& options) 
{
    return invoke_dialog("tk_chooseDirectory", options);
}

} // filedialog

namespace messagebox 
{

static std::string msgbox(const std::string& type, const std::string& icon, const std::string& title, const std::string& message)
{
    auto* interp = interp_map[std::this_thread::get_id()];
    if (!interp) return "";
    return interp->call({"tk_messageBox", "-type", type, "-icon", icon, "-title", title, "-message", message});
}

std::string showinfo(const std::string& title, const std::string& message) 
{
    return msgbox("ok", "info", title, message);
}

std::string showwarning(const std::string& title, const std::string& message) 
{
    return msgbox("ok", "warning", title, message);
}

std::string showerror(const std::string& title, const std::string& message) 
{
    return msgbox("ok", "error", title, message);
}

std::string askquestion(const std::string& title, const std::string& message) 
{
    return msgbox("yesno", "question", title, message);
}

bool askyesno(const std::string& title, const std::string& message) 
{
    return askquestion(title, message) == "yes";
}

bool askokcancel(const std::string& title, const std::string& message) 
{
    return msgbox("okcancel", "question", title, message) == "ok";
}

bool askretrycancel(const std::string& title, const std::string& message)
{
    return msgbox("retrycancel", "warning", title, message) == "retry";
}

bool askyesnocancel(const std::string& title, const std::string& message)
{
    return msgbox("yesnocancel", "question", title, message) == "yes";
}

} // messagebox

} // cpp_tk