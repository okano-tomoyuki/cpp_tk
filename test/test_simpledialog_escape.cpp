// Regression test for the Escape-key cancellation path of cpp_tk::custom::simpledialog::askstring().
// See the comment at the top of test_simpledialog_return.cpp for why this is kept in its own
// .cpp/executable (running askstring() twice in a row within the same process was found to hang).
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "cpp_tk.hpp"
#include "custom.hpp"

namespace tk     = cpp_tk;
namespace custom = tk::custom;

TEST_CASE("simpledialog::askstring: pressing Escape cancels the dialog")
{
    tk::Tk root;
    root.geometry("1x1-3000-3000");

    root.after(100, [&root]() {
        auto dialogs = root.winfo_children();
        if (!dialogs.empty())
        {
            auto dialog = root.nametowidget(dialogs.back());
            dialog.event_generate("<Escape>");
        }
    });

    bool cancelled = false;
    auto result = custom::simpledialog::askstring(root, "Title", "Prompt:", "initial", &cancelled);

    CHECK(cancelled);
    CHECK(result.empty());
}
