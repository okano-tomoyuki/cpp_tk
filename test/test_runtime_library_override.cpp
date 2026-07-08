#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "cpp_tk.hpp"

namespace tk = cpp_tk;

// Without this negative-control test, in an environment where CPP_TK_PACKAGE_RUNTIME_SCRIPTS
// has placed real tcl8.6/tk8.6 directories next to bin/, initialization could succeed by
// accident (e.g. via Tcl's own relative-path search) even if set_runtime_library_paths() did
// nothing at all, making it impossible to confirm the override actually takes effect. Verify
// first that a nonexistent path reliably fails (an Error is thrown).
TEST_CASE("set_runtime_library_paths: a nonexistent directory reliably fails "
          "(confirms there is no fallback to the default search)")
{
    tk::set_runtime_library_paths(
        "C:/definitely_does_not_exist_xyz123/tcl_bogus",
        "C:/definitely_does_not_exist_xyz123/tk_bogus"
    );

    CHECK_THROWS_AS(tk::Tk(), tk::Error);
}

TEST_CASE("set_runtime_library_paths: Tcl_Init()/Tk_Init() succeed using only Tcl/Tk runtime "
          "scripts copied to a location different from the system's own")
{
#if CPP_TK_TEST_RUNTIME_FIXTURE_AVAILABLE
    // Must be called before this process constructs its first cpp_tk object (Interpreter is a
    // lazily-created singleton, and this setting is only consulted when it is constructed).
    tk::set_runtime_library_paths(CPP_TK_TEST_FIXTURE_TCL_LIBRARY, CPP_TK_TEST_FIXTURE_TK_LIBRARY);

    tk::Tk root;
    root.withdraw();
    CHECK_NOTHROW(tk::Label(root, {{"text", "ok"}}).pack());
#else
    WARN("Skipping this test because CPP_TK_RUNTIME_TCL_LIBRARY/CPP_TK_RUNTIME_TK_LIBRARY could "
         "not be detected (e.g. wish was not found).");
#endif
}
