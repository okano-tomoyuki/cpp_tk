// 複数のワーカースレッドから一定間隔でtk::Textへ行を追記するサンプル。
//
// Python本家のtkinterと異なり、cpp_tkはTcl_Interpの生成スレッド以外からWidget/Var等の
// メソッドを直接呼び出すとErrorを送出する(既定のErrorPolicy::STRICT下)。以下のような
// コードは別スレッドから直接ウィジェットを触っているためコンパイルは通るが実行時に
// 例外になる:
//
//     text.insert(tk::END, "from worker\n"); // ワーカースレッドの中で呼ぶとError
//
// 安全に行うには、InterpreterClient::post()でメインスレッド(Tclのイベントループを
// 回しているスレッド)へ処理を依頼する。post()はどのスレッドから呼び出しても安全
// (Tcl_ThreadQueueEvent/Tcl_ThreadAlertを使う)。

#include "cpp_tk.hpp"
#include <atomic>
#include <chrono>
#include <sstream>
#include <thread>

int main()
{
    namespace tk = cpp_tk;

    tk::Tk root;
    root.title("multithread_text");
    root.geometry("480x320");

    tk::Frame frame(root);
    frame.pack({{"fill", "both"}, {"expand", "true"}});

    tk::Text text(frame);
    text.pack({{"side", "left"}, {"fill", "both"}, {"expand", "true"}});

    tk::Scrollbar scrollbar(frame);
    scrollbar.pack({{"side", "right"}, {"fill", "y"}});

    text.yscrollcommand([&scrollbar](const std::string& args) {
        scrollbar.set(args);
    });
    scrollbar.command([&text](const std::string& args) {
        text.yview(args);
    });

    std::atomic<bool> running{true};

    // ワーカースレッド本体。thread_idごとに異なる間隔で行を追記し続ける。
    // textへのアクセスは必ずpost()経由にする(直接呼ぶと別スレッドからのアクセスとしてErrorになる)。
    auto worker = [&text, &running](int thread_id, int interval_ms) {
        int count = 0;
        while (running.load())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms));
            if (!running.load())
                break;

            ++count;
            std::ostringstream oss;
            oss << "[thread " << thread_id << "] line " << count << " (every " << interval_ms << "ms)\n";
            std::string line = oss.str();

            text.post([&text, line]() {
                text.insert(tk::END, line);
                text.see(tk::END);
            });
        }
    };

    // 間隔の異なる2つのワーカースレッドから同時に追記させ、post()経由なら
    // 複数スレッドからでも安全に同じウィジェットを更新できることを示す。
    std::thread worker1(worker, 1, 300);
    std::thread worker2(worker, 2, 700);

    root.mainloop(); // ウィンドウを閉じるとquit()が呼ばれここへ戻る

    running = false;
    worker1.join();
    worker2.join();

    return 0;
}
