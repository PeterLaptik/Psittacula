#include "default_rule_provider.h"
#include "working_dir.h"
#include <fstream>
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
    return R"(You are a helpful coding assistant.
You can operate with a provided multi-tool environment. 


END OF RULES)";
}
