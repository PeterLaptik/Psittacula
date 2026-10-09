#include "test_framework.h"
#include "utf8_util.h"

using utf8::IsValid;
using utf8::Sanitize;

PS_TEST(valid_ascii)
{
    PS_CHECK(IsValid("hello world 123"));
    PS_CHECK(IsValid(""));
}

PS_TEST(valid_multibyte)
{
    PS_CHECK(IsValid("\xD0\xBF\xD1\x80\xD0\xB8\xD0\xB2\xD0\xB5\xD1\x82")); // "привет"
    PS_CHECK(IsValid("\xE4\xB8\xAD\xE6\x96\x87"));                        // "中文"
    PS_CHECK(IsValid("\xF0\x9F\x98\x80"));                                // U+1F600 emoji
}

PS_TEST(invalid_continuation)
{
    PS_CHECK(!IsValid("\xC3"));        // truncated 2-byte
    PS_CHECK(!IsValid("\xC3\x28"));    // bad continuation
    PS_CHECK(!IsValid("\xE2\x82"));    // truncated 3-byte
    PS_CHECK(!IsValid("\xF0\x9F\x98")); // truncated 4-byte
}

PS_TEST(invalid_overlong_and_surrogates)
{
    PS_CHECK(!IsValid("\xC0\xAF"));     // overlong '/'
    PS_CHECK(!IsValid("\xE0\x80\xAF")); // overlong
    PS_CHECK(!IsValid("\xED\xA0\x80")); // surrogate D800
    PS_CHECK(!IsValid("\xF4\x90\x80\x80")); // beyond U+10FFFF
}

PS_TEST(sanitize_valid_passthrough)
{
    // No copy/modify on already-clean text
    PS_CHECK(Sanitize("plain") == "plain");
    PS_CHECK(Sanitize("\xD0\xB0\xD0\xB1\xD0\xB2") == "\xD0\xB0\xD0\xB1\xD0\xB2");
}

PS_TEST(sanitize_replaces_bad_bytes)
{
    const std::string bad = "ab\xCD";
    const std::string good = "ab\xEF\xBF\xBD"; // U+FFFD
    PS_CHECK(Sanitize(bad) == good);
}

PS_TEST(sanitize_replaces_each_bad_byte_individually)
{
    // Two independent invalid bytes -> two replacements
    PS_CHECK(Sanitize(std::string("\xCD\xCD", 2)) ==
        std::string("\xEF\xBF\xBD\xEF\xBF\xBD", 6));
}

PS_TEST(sanitize_truncated_tail_becomes_single_replacement)
{
    PS_CHECK(Sanitize(std::string("ok\xD0\xBF\xD1", 5)) ==
        std::string("ok\xD0\xBF\xEF\xBF\xBD"));
}

PS_TEST(sanitize_nul_survives_inside)
{
    // NUL is a valid 1-byte code point; Sanitize keeps it (writer escapes later)
    const std::string with_nul = std::string("a\0b", 3);
    PS_CHECK(Sanitize(with_nul) == with_nul);
}

PS_TEST(console_to_utf8_passthrough_when_utf8)
{
    PS_CHECK(utf8::ConsoleToUtf8("plain text") == "plain text");
}

PS_TEST(ansi_to_utf8_passthrough_when_utf8)
{
    PS_CHECK(utf8::AnsiToUtf8("plain text") == "plain text");
}
