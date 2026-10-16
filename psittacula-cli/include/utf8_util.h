#ifndef PSITTACULA_UTF8_UTIL_H_INCLUDED
#define PSITTACULA_UTF8_UTIL_H_INCLUDED

#include <string>

#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
// windows.h defines GetObject as a macro (GetObjectA/W), which clashes with
// rapidjson::GenericValue::GetObject()
#ifdef GetObject
#undef GetObject
#endif
#endif

/// UTF-8 helpers.
/// Any text that goes into a request body must be a well-formed UTF-8 sequence:
/// servers (llama.cpp, OpenAI-compatible gateways) parse bodies with nlohmann::json
/// and reject the whole request with 'parse_error.101 ... ill-formed UTF-8 byte'
/// when a single invalid byte slips in (e.g. CP866 stderr of cmd.exe).
namespace utf8
{
    /// Number of bytes in a sequence started by 'lead', 0 when the lead byte is invalid
    inline size_t SequenceLength(unsigned char lead)
    {
        if (lead < 0x80) return 1;
        if ((lead & 0xE0) == 0xC0) return 2;
        if ((lead & 0xF0) == 0xE0) return 3;
        if ((lead & 0xF8) == 0xF0) return 4;
        return 0;
    }

    /// Checks whether the whole string is a well-formed UTF-8 sequence
    inline bool IsValid(const std::string &text)
    {
        const size_t size = text.size();
        size_t i = 0;

        while (i < size)
        {
            unsigned char lead = static_cast<unsigned char>(text[i]);
            size_t len = SequenceLength(lead);

            if (len == 0 || i + len > size)
                return false;

            unsigned int code_point = lead & (0xFFu >> (len + 1));

            for (size_t k = 1; k < len; ++k)
            {
                unsigned char cont = static_cast<unsigned char>(text[i + k]);
                if ((cont & 0xC0) != 0x80)
                    return false;
                code_point = (code_point << 6) | (cont & 0x3F);
            }

            // overlong forms, surrogates and out-of-range code points are ill-formed
            static const unsigned int min_code_points[5] = { 0, 0, 0x80, 0x800, 0x10000 };
            if (code_point < min_code_points[len] || code_point > 0x10FFFF ||
                (code_point >= 0xD800 && code_point <= 0xDFFF))
                return false;

            i += len;
        }

        return true;
    }

    /// Replaces every ill-formed byte (and every truncated trailing sequence) with U+FFFD
    inline std::string Sanitize(const std::string &text)
    {
        if (IsValid(text))
            return text;

        static const char replacement[] = "\xEF\xBF\xBD";

        std::string out;
        out.reserve(text.size());

        const size_t size = text.size();
        size_t i = 0;

        while (i < size)
        {
            unsigned char lead = static_cast<unsigned char>(text[i]);
            size_t len = SequenceLength(lead);

            if (len == 1)
            {
                out.push_back(text[i]);
                ++i;
                continue;
            }

            bool ok = (len > 1) && (i + len <= size);
            unsigned int code_point = lead & (0xFFu >> (len + 1));

            for (size_t k = 1; ok && k < len; ++k)
            {
                unsigned char cont = static_cast<unsigned char>(text[i + k]);
                if ((cont & 0xC0) != 0x80)
                {
                    ok = false;
                    break;
                }
                code_point = (code_point << 6) | (cont & 0x3F);
            }

            static const unsigned int min_code_points[5] = { 0, 0, 0x80, 0x800, 0x10000 };
            if (ok && (code_point < min_code_points[len] || code_point > 0x10FFFF ||
                (code_point >= 0xD800 && code_point <= 0xDFFF)))
                ok = false;

            if (ok)
            {
                out.append(text, i, len);
                i += len;
            }
            else
            {
                out.append(replacement, sizeof(replacement) - 1);
                ++i;
            }
        }

        return out;
    }

    /// Converts text written by a child process to UTF-8.
    /// On Windows console programs write in the console code page (CP866/CP1251/...),
    /// which is not UTF-8; such text is decoded to wide chars and re-encoded as UTF-8.
    /// Text that is already well-formed UTF-8 (chcp 65001, cross-platform tools) is kept as is.
    inline std::string ConsoleToUtf8(const std::string &text)
    {
        if (text.empty() || IsValid(text))
            return text;

#ifdef _WIN32
        UINT cp = GetConsoleOutputCP();
        if (cp == 0 || cp == CP_UTF8)
            cp = GetOEMCP();

        if (cp == 0 || cp == CP_UTF8)
            return Sanitize(text);

        int wide_len = MultiByteToWideChar(cp, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
        if (wide_len <= 0)
            return Sanitize(text);

        std::wstring wide(static_cast<size_t>(wide_len), L'\0');
        if (MultiByteToWideChar(cp, 0, text.data(), static_cast<int>(text.size()), &wide[0], wide_len) <= 0)
            return Sanitize(text);

        int utf8_len = WideCharToMultiByte(CP_UTF8, 0, wide.data(), wide_len, nullptr, 0, nullptr, nullptr);
        if (utf8_len <= 0)
            return Sanitize(text);

        std::string out(static_cast<size_t>(utf8_len), '\0');
        if (WideCharToMultiByte(CP_UTF8, 0, wide.data(), wide_len, &out[0], utf8_len, nullptr, nullptr) <= 0)
            return Sanitize(text);

        return out;
#else
        return Sanitize(text);
#endif
    }
    /// Converts text stored in the system ANSI code page (CP1251 / CP1252 / ...) to UTF-8.
    /// Text that is already well-formed UTF-8 is kept as is.
    inline std::string AnsiToUtf8(const std::string &text)
    {
        if (text.empty() || IsValid(text))
            return text;

#ifdef _WIN32
        UINT cp = GetACP();
        if (cp == 0 || cp == CP_UTF8)
            return Sanitize(text);

        int wide_len = MultiByteToWideChar(cp, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
        if (wide_len <= 0)
            return Sanitize(text);

        std::wstring wide(static_cast<size_t>(wide_len), L'\0');
        if (MultiByteToWideChar(cp, 0, text.data(), static_cast<int>(text.size()), &wide[0], wide_len) <= 0)
            return Sanitize(text);

        int utf8_len = WideCharToMultiByte(CP_UTF8, 0, wide.data(), wide_len, nullptr, 0, nullptr, nullptr);
        if (utf8_len <= 0)
            return Sanitize(text);

        std::string out(static_cast<size_t>(utf8_len), '\0');
        if (WideCharToMultiByte(CP_UTF8, 0, wide.data(), wide_len, &out[0], utf8_len, nullptr, nullptr) <= 0)
            return Sanitize(text);

        return out;
#else
        return Sanitize(text);
#endif
    }
}

#endif // PSITTACULA_UTF8_UTIL_H_INCLUDED
