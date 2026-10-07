#ifndef CURL_GLOBAL_GUARD_INCLUDED_H
#define CURL_GLOBAL_GUARD_INCLUDED_H

#include <curl/curl.h>
#include <atomic>
#include <mutex>

/// Process-wide libcurl global state lifecycle.
///
/// curl_global_init / curl_global_cleanup initialize and tear down global
/// state for the whole process and 'should be called exactly once'. Multiple
/// HttpClient objects and web_fetch calls live at the same time, so doing it
/// per object (or per call) tears down state still used by other handles -
/// undefined behaviour.
///
/// This helper is refcounted and shared across all modules that include it:
/// the first acquire() initializes the library, the last release() cleans it
/// up. Scope the Guard object (or pair acquire / release) around the whole
/// lifetime of curl easy handles.
namespace curl_global
{
    inline std::mutex& Mutex()
    {
        static std::mutex m;
        return m;
    }

    inline std::atomic<int>& RefCount()
    {
        static std::atomic<int> count{ 0 };
        return count;
    }

    /// Acquires the global state (initializes the library on first use).
    /// Returns false only when curl_global_init failed; in that case do NOT
    /// call Release() (the refcount was not changed and the next Acquire
    /// retries the initialization).
    inline bool Acquire()
    {
        std::lock_guard<std::mutex> lock(Mutex());

        if (RefCount().load() > 0)
        {
            RefCount().fetch_add(1);
            return true;
        }

        if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK)
            return false;

        RefCount().store(1);
        return true;
    }

    /// Releases the global state (cleans up the library on the last release).
    /// Pair with a successful Acquire() only.
    inline void Release()
    {
        std::lock_guard<std::mutex> lock(Mutex());

        if (RefCount().fetch_sub(1) == 1)
            curl_global_cleanup();
    }

    /// RAII wrapper: acquires in the constructor, releases in the destructor.
    /// Keep it alive while any curl easy handle is in use (a scoped variable
    /// declared before curl_easy_init covers all exit paths automatically).
    class Guard
    {
        public:
            Guard() : m_acquired(Acquire())
            { }

            ~Guard()
            {
                if (m_acquired)
                    Release();
            }

            Guard(const Guard &) = delete;
            Guard &operator=(const Guard &) = delete;

        private:
            bool m_acquired;
    };
}

#endif // CURL_GLOBAL_GUARD_INCLUDED_H
