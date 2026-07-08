// Regression tests for cross-thread access detection (phase 1) and InterpreterClient::post() (phase 2).
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "cpp_tk.hpp"

#include <thread>
#include <chrono>
#include <atomic>
#include <vector>

namespace tk = cpp_tk;

TEST_CASE("post(): a worker thread's post() safely wakes mainloop (vwait forever) and runs the job")
{
    tk::set_error_policy(tk::ErrorPolicy::DEFAULT);
    tk::Tk root;
    root.withdraw();

    tk::Label label(root);
    label.text("initial").pack();

    bool job_ran = false;

    std::thread worker([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        // Touching a widget directly from a worker thread is forbidden (throws Error for
        // cross-thread access), but post() lets it run safely on the main thread instead.
        label.post([&]() {
            job_ran = true;
            label.text("updated from worker thread");
            root.quit(); // stop mainloop (vwait forever)
        });
    });

    root.mainloop();
    worker.join();

    CHECK(job_ran);
    CHECK(label.cget("text") == "updated from worker thread");
}

TEST_CASE("post(): multiple worker threads can post safely")
{
    tk::set_error_policy(tk::ErrorPolicy::DEFAULT);
    tk::Tk root;
    root.withdraw();

    tk::Text text(root);
    text.pack();

    std::atomic<int> jobs_done{0};
    constexpr int worker_count = 3;

    std::vector<std::thread> workers;
    for (int i = 0; i < worker_count; ++i)
    {
        workers.emplace_back([&, i]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(50 + i * 20));
            text.post([&, i]() {
                text.insert(tk::END, "line from thread " + std::to_string(i) + "\n");
                if (++jobs_done == worker_count)
                    root.quit();
            });
        });
    }

    root.mainloop();
    for (auto& w : workers) w.join();

    CHECK(jobs_done == worker_count);
}
