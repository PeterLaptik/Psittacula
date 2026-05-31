#include "working_dir.h"
#include "console_writer.h"
#include "dialogue_body.h"
#include <iostream>
#include <filesystem>

using console::TextOrigin;

namespace fs = std::filesystem;

const char * const kDefaultDir = "Psittacula";
const char * const kModelsSubDir = "models";

WorkingDir& WorkingDir::GetInstance()
{
    static WorkingDir instance;
    if (instance.m_workdir.empty())
    {
        instance.CreateWorkingDirs();
        std::string msg = instance.m_fmt.Format("No workdir path reveived. Using default directory: %?", instance.m_workdir);
        console::write_line(msg, TextOrigin::filesystem);
        instance.FindModels();
    }

    return instance;
}

void WorkingDir::SetSettingsDir(const std::string &path)
{
    WorkingDir &instance = WorkingDir::GetInstance();
    instance.m_workdir = path;

    // Create subdirectories if they don't exist
    fs::path base = instance.m_workdir;
    fs::path models = base / "models";
    fs::path settings = base / "settings";
    fs::path projects = base / "projects";
    fs::create_directories(models);
    fs::create_directories(settings);
    fs::create_directories(projects);

    instance.FindModels();
}

void WorkingDir::SetWorkspaceDir(const std::string &path)
{
    WorkingDir &instance = WorkingDir::GetInstance();
    instance.m_workspace = path;
}

void WorkingDir::UpdateModels()
{
    models_list.clear();
    FindModels();
}

const std::vector<std::string>& WorkingDir::GetModelsList() const
{
    return models_list;
}

const std::string WorkingDir::GetProjectDir() const
{
    return m_workdir + '/' + "projects/";
}

const std::string WorkingDir::GetModelsDir() const
{
    return m_workdir + '/' + "models/";
}

const std::string WorkingDir::GetSettingsDir() const
{
    return m_workdir + '/' + "settings/";
}

void WorkingDir::FindModels()
{
    try
    {
        // Root dir
        fs::directory_entry root_dir(m_workdir);
        if(!root_dir.exists() || !root_dir.is_directory())
        {
            console::write_line("Workdir is not found or is not a directory!", TextOrigin::error);
            return;
        }

        // Models dir
        fs::directory_entry models_dir;
        for (const auto &entry : fs::directory_iterator(root_dir)) {
            if (entry.is_directory() && entry.path().filename() == kModelsSubDir) {
                models_dir = entry;
            }
        }

        if (models_dir.path().filename().empty())
        {
            std::string msg = m_fmt.Format("Models directory is not found in '%?'", root_dir.path().string());
            console::write_line(msg, TextOrigin::error);
            return;
        }

        // Models list
        for (const auto &entry : fs::recursive_directory_iterator(models_dir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".txt") {
                models_list.push_back(entry.path().string());
            }
        }
    }
    catch (const std::exception &e)
    {
        console::write_line("Error on searching models:", TextOrigin::error);
        console::write_line(e.what(), TextOrigin::error);
    }
}

void WorkingDir::CreateWorkingDirs()
{
    fs::path home_dir = "";

#ifdef _WIN32
    const char *home = std::getenv("USERPROFILE");
    if (home) 
    {
        const char *drive = std::getenv("HOMEDRIVE");
        const char *path = std::getenv("HOMEPATH");
        if (drive && path)
            home_dir = fs::path(drive) / path / "Documents" / kDefaultDir;
    }
#else
    const char *home = std::getenv("HOME");
    home_dir = fs::path(home ? home / kDefaultDir : "");
#endif

    if (!fs::exists(home_dir))
    {
        console::write_line(m_fmt.Format("Creating working dir: %?", home_dir.string()), TextOrigin::filesystem);
        std::error_code ec;
        fs::create_directories(home_dir, ec);
        if (ec) 
        {
            console::write_line(m_fmt.Format("Failed to create directory: %?", ec.message()), TextOrigin::error);
            return;
        }
    }

    m_workdir = home_dir.string();

    fs::path base = m_workdir;
    fs::path models = base / "models";
    fs::path settings = base / "settings";
    fs::path projects = base / "projects";
    fs::create_directories(models);
    fs::create_directories(settings);
    fs::create_directories(projects);
}

bool WorkingDir::IsInWorkDir(const std::string &path) const
{
    namespace fs = std::filesystem;

    try
    {
        fs::path sandbox = fs::weakly_canonical(m_workspace);
        fs::path target = fs::weakly_canonical(path);

        // Windows is case-insensitive: normalize to lowercase
#ifdef _WIN32
        auto to_lower = [](std::string s) {
            std::transform(s.begin(), s.end(), s.begin(),
                [](unsigned char c) { return std::tolower(c); });
            return s;
            };

        std::string sandbox_str = to_lower(sandbox.string());
        std::string target_str = to_lower(target.string());
#else
        std::string sandbox_str = sandbox.string();
        std::string target_str = target.string();
#endif

        // Ensure trailing separator for correct prefix matching
        if (!sandbox_str.empty() && sandbox_str.back() != '\\' && sandbox_str.back() != '/')
            sandbox_str += fs::path::preferred_separator;

        return target_str.rfind(sandbox_str, 0) == 0; // prefix check
    }
    catch (...)
    {
        return false; // invalid path, treat as outside sandbox
    }
}
