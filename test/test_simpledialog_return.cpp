// Regression test for the Return-key confirmation path of cpp_tk::custom::simpledialog::askstring().
// Kept in its own .cpp/executable because askstring() creates a modal dialog with grab_set(),
// and running it back-to-back with the similar test in test_simpledialog_escape.cpp in the same
// process was found to hang on the second wait_window() (regardless of run order) - it never
// returns (see the addendum to the 5th inventory pass in docs/tasks.md). This is presumed to be
// caused by grab-related platform state carrying over into a second Tk_Init cycle within a
// single process, rather than a bug in cpp_tk itself; if anything it confirms the existing
// one-process-per-test-file policy (see the comment at the top of test/CMakeLists.txt) is the
// right call, so it is simply avoided by keeping these in separate processes.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "cpp_tk.hpp"
#include "custom.hpp"

namespace tk     = cpp_tk;
namespace custom = tk::custom;

TEST_CASE("simpledialog::askstring: pressing Return returns the confirmed input value")
{
    tk::Tk root;
    root.geometry("1x1-3000-3000"); // withdraw() would prevent synthetic events from being delivered, so place off-screen instead

    // The dialog is created internally inside the askstring() call, so there is no direct handle
    // to it from outside. Re-enter the event loop via root.after(), walk the widget tree to set
    // a value into the Entry, and fire <Return> to simulate confirming with OK.
    // Note: this callback is invoked from the Tcl side (exceptions are swallowed via
    // invoke_guarded), so using doctest macros like REQUIRE/CHECK inside it (which throw on
    // failure) would leave the dialog open forever and hang wait_window(). So nothing inside
    // throws; even in an unexpected state the dialog is always closed so wait_window() returns,
    // and the actual assertions are left to the caller's CHECK below.
    root.after(100, [&root]() {
        auto dialogs = root.winfo_children();
        if (!dialogs.empty())
        {
            auto dialog = root.nametowidget(dialogs.back());
            std::string entry_name;
            for (auto& child : dialog.winfo_children())
            {
                auto w = root.nametowidget(child);
                if (w.winfo_class() == "Entry") { entry_name = child; break; }
            }
            if (!entry_name.empty())
            {
                // Overwrite the Entry's content directly, then fire Return (<Return> is bound on the entry).
                dialog.call({entry_name, "delete", "0", "end"});
                dialog.call({entry_name, "insert", "0", "hello"});
                root.nametowidget(entry_name).event_generate("<Return>");
                return;
            }
            dialog.destroy(); // don't hang even in the unexpected case where no Entry is found
        }
    });

    bool cancelled = true;
    auto result = custom::simpledialog::askstring(root, "Title", "Prompt:", "initial", &cancelled);

    CHECK_FALSE(cancelled);
    CHECK(result == "hello");
}
