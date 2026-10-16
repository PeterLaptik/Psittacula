// Zero-dependency test harness for the Psittacula engine.
//
// Design goals:
//   - no external test framework (no FetchContent / no network at configure)
//   - the test target links psittacula-cli / psittacula-tools SOURCES needed
//     by the engine logic, but performs no networking (no curl handle is
//     ever created here; HttpClient is never constructed)
//
// Adding a case: PS_TEST(fn) registers it automatically via a static
// registrar; PS_CHECK(expr) / PS_CHECK_MSG(expr, msg) abort the case on
// failure with file/line.
#include "test_framework.h"

int main()
{
    return TestRegistry::instance().run_all();
}
