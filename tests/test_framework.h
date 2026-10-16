#ifndef PSITTACULA_TEST_FRAMEWORK_INCLUDED_H
#define PSITTACULA_TEST_FRAMEWORK_INCLUDED_H

#include <cstddef>
#include <cstdio>
#include <string>
#include <vector>

#define PS_CONCAT_INNER(a, b) a##b
#define PS_CONCAT(a, b) PS_CONCAT_INNER(a, b)

/// Registers a test case: PS_TEST(name) { ...body... }
#define PS_TEST(fn_name)                                                        \
    static void fn_name();                                                      \
    static const bool PS_CONCAT(ps_reg_, fn_name) =                             \
        ::TestRegistry::instance().add(#fn_name, &fn_name);                     \
    static void fn_name()

/// Assertions: on failure, report file/line, mark the case failed, and
/// abort the current test function (non-fatal for the whole run)
#define PS_CHECK(expr)                                                          \
    do {                                                                        \
        if (!(expr)) {                                                          \
            ::TestRegistry::current_ok() = false;                               \
            std::printf("    FAIL %s:%d  %s\n", __FILE__, __LINE__, #expr);     \
            return;                                                             \
        }                                                                       \
    } while (false)

#define PS_CHECK_MSG(expr, msg)                                                 \
    do {                                                                        \
        if (!(expr)) {                                                          \
            ::TestRegistry::current_ok() = false;                               \
            std::printf("    FAIL %s:%d  %s\n    %s\n", __FILE__, __LINE__,     \
                #expr, (msg));                                                  \
            return;                                                             \
        }                                                                       \
    } while (false)

struct TestCase
{
    const char *name;
    void (*fn)();
};

class TestRegistry
{
    public:
        static TestRegistry &instance()
        {
            static TestRegistry registry;
            return registry;
        }

        bool add(const char *name, void (*fn)())
        {
            m_tests.push_back(TestCase{ name, fn });
            return true;
        }

        int run_all()
        {
            std::size_t failed = 0;

            for (const TestCase &t : m_tests)
            {
                current_ok() = true;
                std::printf("[ RUN  ] %s\n", t.name);
                t.fn();
                if (current_ok())
                    std::printf("[  OK  ] %s\n", t.name);
                else
                {
                    ++failed;
                    std::printf("[ FAIL ] %s\n", t.name);
                }
            }

            std::printf("%zu test(s), %zu failed\n", m_tests.size(), failed);
            return failed == 0 ? 0 : 1;
        }

        /// The per-case flag PS_CHECK flips; reset by run_all
        static bool &current_ok()
        {
            static bool ok = true;
            return ok;
        }

    private:
        TestRegistry() = default;

        std::vector<TestCase> m_tests;
};

#endif // PSITTACULA_TEST_FRAMEWORK_INCLUDED_H
