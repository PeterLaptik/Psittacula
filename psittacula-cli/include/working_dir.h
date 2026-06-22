#ifndef WORKING_DIR_INCLUDED_H
#define WORKING_DIR_INCLUDED_H

#include "format_util.h"
#include <string>
#include <vector>

/// A singleton keeping working dir paths
class WorkingDir
{
    public:
        static WorkingDir& GetInstance();

        void SetWorkDir(const std::string &path);

        void SetProjectDir(const std::string &path);

        void UpdateModels();

        const std::vector<std::string> &GetModelsList() const;

        std::string GetWorkDir() const;

        std::string GetProjectDir() const;

        std::string GetModelsDir() const;

        std::string GetSettingsDir() const;

        bool IsInWorkDir(const std::string &path) const;

    private:
        WorkingDir() = default;
        ~WorkingDir() = default;

        void CreateWorkingDirs();
        void FindModels();

        Formatter m_fmt;
        std::string m_workdir;      // Settings for model connections, etc
        std::string m_project_dir;  // Project: files and directories can be modified via file tools
        std::vector<std::string> models_list;
};

#endif // WORKING_DIR_INCLUDED_H
