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
        instance.FindModels();
    }

    return instance;
}

void WorkingDir::SetWorkDir(const std::string &path)
{
    WorkingDir &instance = WorkingDir::GetInstance();
    instance.m_workdir = path;

    // Create subdirectories if they don't exist
    fs::path base = instance.m_workdir;
    fs::path models = base / "models";
    fs::path settings = base / "settings";
    fs::path projects = base / "projects";
    fs::path logs = base / "logs";
    fs::create_directories(models);
    fs::create_directories(settings);
    fs::create_directories(projects);
    fs::create_directories(logs);

    m_project_dir = projects.string();

    instance.UpdateModels();
}

void WorkingDir::SetProjectDir(const std::string &path)
{
    m_project_dir = path;
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

std::string WorkingDir::GetProjectDir() const
{
    return m_project_dir.empty() ? m_workdir + '/' + "projects/" : m_project_dir;
}

std::string WorkingDir::GetModelsDir() const
{
    return m_workdir + '/' + "models/";
}

std::string WorkingDir::GetSettingsDir() const
{
    return m_workdir + '/' + "settings/";
}

std::string WorkingDir::GetLogsDir() const
{
    return m_workdir + '/' + "logs/";
}

std::string WorkingDir::GetWorkDir() const
{
    return m_workdir;
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
    fs::path logs = base / "logs";
    fs::create_directories(models);
    fs::create_directories(settings);
    fs::create_directories(projects);
    fs::create_directories(logs);

    m_project_dir = projects.string();
}

bool WorkingDir::IsInWorkDir(const std::string &path) const
{
    namespace fs = std::filesystem;

    try
    {
        fs::path sandbox = fs::weakly_canonical(m_project_dir);
        fs::path target = fs::weakly_canonical(path);

#ifdef _WIN32
        // Normalize case for Windows
        auto normalize = [](const fs::path &p) {
            std::string s = p.string();
            std::transform(s.begin(), s.end(), s.begin(),
                [](unsigned char c) { return std::tolower(c); });
            return fs::path(s);
            };

        sandbox = normalize(sandbox);
        target = normalize(target);
#endif

        // Explicit equality check
        if (target == sandbox)
            return true;

        // Walk upward from target until root or sandbox is found
        int level_counter = 0;
        fs::path cur = target;
        while (!cur.empty() && level_counter < 32)
        {
            if (cur == sandbox)
                return true;

            cur = cur.parent_path();
            level_counter++;
        }

        return false;
    }
    catch (...)
    {
        return false;
    }
}