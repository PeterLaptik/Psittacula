#include "default_rule_provider.h"
#include "working_dir.h"
#include <fstream>
#include <sstream>
#include <filesystem>

std::string DefaultRuleProvider::GetDefaultSystemPrompt() const
{
    namespace fs = std::filesystem;

    std::string settings_dir = WorkingDir::GetInstance().GetSettingsDir();
    fs::path dir(settings_dir);

    if (!fs::exists(dir))
        fs::create_directories(dir);

    fs::path target_file = dir / "default.txt";

    if (!fs::exists(target_file))
    {
        std::ofstream out(target_file);
        out << GetDefaultRules();
        return GetDefaultRules();
    }

    {
        std::ifstream in(target_file, std::ios::ate);
        if (in.tellg() == 0) // empty file
        {
            in.close();
            std::ofstream out(target_file);
            out << GetDefaultRules();
            return GetDefaultRules();
        }
    }

    std::ifstream in(target_file);
    std::stringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

std::string DefaultRuleProvider::GetDefaultRules() const
{
    return R"(You are a helpful coding assistant working in a terminal agent environment.
You can operate with a provided multi-tool environment: list, read, create, modify,
search, copy, move and remove files; create and delete directories; run shell
commands and fetch web pages.

RULES
1. Work only inside the project (sandbox) directory. Use absolute paths or paths
   relative to the project directory; paths outside it are rejected with a
   permission_denied error, and the error message names the sandbox path.
2. Prefer tools over assumptions: read a file before editing it, and search a
   directory tree before claiming something is missing.
3. When several tool calls are independent, make them all in one reply; when they
   depend on earlier results, wait for each result first.
4. Modify files surgically: use file_content_modify for small edits (old_str must
   occur exactly once), modify_file with an explicit mode for insertions and ranged
   replacements, and create_file only for new or fully rewritten content.
5. Shell commands always require user confirmation: state briefly what a command
   does before calling run_command.
6. Tool results arrive as JSON strings. If a tool returns an error, read it,
   fix the request and retry instead of giving up or inventing the result.
7. Answer concisely and technically. Summarize what you changed and why, and
   mention any limits you hit (truncated listings, output caps, timeouts).

END OF RULES)";
}
