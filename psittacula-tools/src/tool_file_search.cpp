#include "tool_file_search.h"
#include "console_writer.h"
#include "format_util.h"
#include "working_dir.h"
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>
#include <algorithm>
#include <cctype>

namespace {

    // Output is capped on three axes so a huge tree cannot produce a giant
    // tool response: total matches kept, total JSON payload bytes and
    // per-file matches (the per-file cap also stops a pathological single
    // file, e.g. minified JS with thousands of hits, from filling the whole
    // budget alone)
    constexpr size_t kMaxMatches = 200;
    constexpr size_t kMaxResultBytes = 96 * 1024;
    constexpr size_t kMaxMatchesPerFile = 25;
    constexpr size_t kSnippetMaxBytes = 512;

    // ASCII-only lowering for case-insensitive matching. Byte-wise ::tolower
    // (with an unsigned char argument) leaves all bytes >= 0x80 unchanged in
    // the C locale, so UTF-8 sequences pass through intact: the match stays
    // ASCII-case-insensitive and never corrupts Cyrillic/emoji text.
    std::string LowerAsciiCopy(const std::string &s)
    {
        std::string out(s);
        std::transform(out.begin(), out.end(), out.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return out;
    }

    // Cheap binary sniff: NUL-byte detection plus a share of non-printable
    // bytes in the probe. Purely heuristic: unknown formats stay searchable,
    // clearly binary content is skipped.
    bool LooksBinary(const std::string &chunk)
    {
        size_t suspicious = 0;

        for (const unsigned char c : chunk)
        {
            if (c == 0x00 || c == 0xFF || c == 0x1A)
            {
                ++suspicious;
                continue;
            }

            if (c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v')
                continue;

            if (c < 0x20)
                ++suspicious;
        }

        return suspicious > chunk.size() / 4;
    }

    // Clips to max_bytes without splitting a UTF-8 sequence (a split tail
    // would be turned into U+FFFD by the sanitize gate - avoidable damage)
    std::string ClipUtf8(const std::string &s, size_t max_bytes)
    {
        if (s.size() <= max_bytes)
            return s;

        size_t end = max_bytes;
        while (end > 0 && (static_cast<unsigned char>(s[end]) & 0xC0) == 0x80)
            --end;

        return s.substr(0, end);
    }

} // namespace

void FileSearchTool::GetParameters(std::vector<ToolParameter> &params_acc)
{
    params_acc.push_back({
        "query",
        "string",
        "Plain-text search query.",
        false
        });

    params_acc.push_back({
        "regex",
        "string",
        "Regex pattern to match.",
        false
        });

    params_acc.push_back({
        "regex_mode",
        "boolean",
        "If true, use regex pattern instead of plain-text search.",
        false
        });

    params_acc.push_back({
        "path",
        "string",
        "Directory path to search.",
        true
        });

    params_acc.push_back({
        "case_sensitive",
        "boolean",
        "If true, search is case-sensitive.",
        false
        });
}

std::string FileSearchTool::Execute(std::vector<ToolParameter> &params_values)
{
    Formatter fmt;
    WorkingDir &wdir = WorkingDir::GetInstance();

    console::write_line("File search tool.", console::TextOrigin::tools);

    std::string path = GetParam(params_values, "path");
    UnEscapeSlashesInPath(path);

    const bool case_sensitive = GetParamBool(params_values, "case_sensitive", false);
    const bool regex_mode = GetParamBool(params_values, "regex_mode", false);

    const std::string query = GetParam(params_values, "query");
    const std::string regex_pattern = GetParam(params_values, "regex");

    console::write_line(query.empty() ? ("Pattern: " + regex_pattern) : ("Query: " + query), console::TextOrigin::tools);

    if (path.empty())
    {
        return R"({"error":{"type":"invalid_arguments","message":"Missing required parameter: path"}})";
    }

    path = ResolveProjectPath(path);

    if (!wdir.IsInWorkDir(path))
    {
        console::write_line(fmt.Format("Permission_denied: path is outside working directory:\n path: %?\n working directory: %?", path, wdir.GetProjectDir()), console::TextOrigin::error);
        return fmt.Format("{\"error\":{\"type\":\"permission_denied\",\"message\":\"Path is outside working directory (%?)\",\"path\":\"%?\"}}", wdir.GetProjectDir(), path);
    }

    if (regex_mode)
    {
        if (regex_pattern.empty())
        {
            return R"({"error":{"type":"invalid_arguments","message":"regex_mode=true but no regex pattern provided"}})";
        }
    }
    else
    {
        if (query.empty())
        {
            return R"({"error":{"type":"invalid_arguments","message":"Missing required parameter: query"}})";
        }
    }

    console::write_line("Search path: " + path, console::TextOrigin::tools);

    // Compile once per tool call, explicitly, with a clean error message
    // instead of an unhandled std::regex_error killing the whole call
    std::regex re;
    if (regex_mode)
    {
        try
        {
            const auto flags = case_sensitive ? std::regex::ECMAScript
                : (std::regex::ECMAScript | std::regex::icase);
            re.assign(regex_pattern, flags);
        }
        catch (const std::regex_error &e)
        {
            console::write_line(fmt.Format("Invalid regex: %?", e.what()), console::TextOrigin::error);
            return fmt.Format("{\"error\":{\"type\":\"invalid_arguments\",\"message\":\"Invalid regex pattern: %?\"}}", std::string(e.what()));
        }
    }

    bool truncated = false;

    auto matches = SearchDirectory(
        path,
        regex_mode ? regex_pattern : LowerAsciiCopy(query),
        case_sensitive,
        regex_mode,
        regex_mode ? &re : nullptr,
        truncated
    );

    // JSON assembly: every string value goes through GetEscapedJSONString
    // (a rapidjson writer). The old version interpolated raw strings, so a
    // snippet containing a quote or a backslash produced invalid JSON and
    // the whole tool response was silently corrupted.
    std::ostringstream json;
    json << "{\"success\":true,\"result\":[";

    const size_t kept = std::min(matches.size(), kMaxMatches);

    for (size_t i = 0; i < kept; ++i)
    {
        const auto &m = matches[i];

        if (i > 0)
            json << ",";

        json << "{\"file\":" << GetEscapedJSONString(m.file)
             << ",\"line\":" << m.line
             << ",\"snippet\":" << GetEscapedJSONString(m.snippet)
             << "}";
    }

    if (truncated)
        json << "],\"truncated\":true,\"message\":";
    else
        json << "],\"message\":";

    json << GetEscapedJSONString(fmt.Format(
        "Found %? match%?%?",
        matches.size(),
        matches.size() == 1 ? "" : "es",
        truncated ? "; output truncated (too many matches or too large payload)" : ""))
        << "}";

    return json.str();
}

std::vector<FileSearchTool::Match> FileSearchTool::SearchDirectory(
    const std::string &root,
    const std::string &pattern,
    bool case_sensitive,
    bool use_regex,
    const std::regex *re,
    bool &truncated)
{
    std::vector<Match> results;

    std::error_code walk_ec;
    std::filesystem::recursive_directory_iterator it(root, walk_ec), end;

    if (walk_ec)
        return results; // root unreadable: empty result, not a crash

    std::error_code ec;
    bool stop = false;
    size_t result_bytes = 0;

    // Errors during the walk (unreadable subdirectory, deletion race) must
    // not kill the whole search: increment(ec) skips the broken entry and
    // the loop continues. increment() is called exactly once per iteration.
    while (!walk_ec && !stop && it != end)
    {
        ec.clear();

        const auto &entry = *it;
        const std::string file_path = entry.path().string();
        const bool is_file = entry.is_regular_file(ec);

        if (!ec && is_file)
        {
            // Binary sniff on the first 4KB; unreadable files are skipped
            {
                std::ifstream in(file_path, std::ios::binary);
                if (!in)
                {
                    it.increment(walk_ec);
                    continue;
                }

                char head[4096] = {};
                in.read(head, sizeof(head));
                if (LooksBinary(std::string(head, static_cast<size_t>(in.gcount()))))
                {
                    it.increment(walk_ec);
                    continue;
                }
            }

            auto file_matches = SearchFile(file_path, pattern, case_sensitive, use_regex, re);

            for (auto &m : file_matches)
            {
                if (results.size() >= kMaxMatches || result_bytes >= kMaxResultBytes)
                {
                    truncated = true;
                    stop = true;
                    break;
                }

                result_bytes += m.file.size() + m.snippet.size() + 24;
                results.push_back(std::move(m));
            }
        }

        if (!stop)
            it.increment(walk_ec);
    }

    return results;
}

std::vector<FileSearchTool::Match> FileSearchTool::SearchFile(
    const std::string &path,
    const std::string &pattern,
    bool case_sensitive,
    bool use_regex,
    const std::regex *re)
{
    std::vector<Match> results;
    std::ifstream file(path);
    if (!file.is_open())
        return results;

    const std::string needle_lower = case_sensitive ? pattern : LowerAsciiCopy(pattern);

    std::string line;
    int line_number = 0;

    while (std::getline(file, line))
    {
        line_number++;

        bool match = false;

        if (use_regex)
        {
            try
            {
                match = std::regex_search(line, *re);
            }
            catch (const std::regex_error &)
            {
                return results; // pathological line/pattern interplay: give up on this file
            }
        }
        else
        {
            const std::string hay = case_sensitive ? line : LowerAsciiCopy(line);
            match = hay.find(needle_lower) != std::string::npos;
        }

        if (match)
        {
            results.push_back({ path, line_number, ClipUtf8(line, kSnippetMaxBytes) });

            if (results.size() >= kMaxMatchesPerFile)
                break; // this file contributed enough
        }
    }

    return results;
}
