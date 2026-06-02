#ifndef FILES_INCLUDED_H
#define FILES_INCLUDED_H

#include "format_util.h"
#include <string>
#include <vector>

class WorkingDir
{
    public:
        static WorkingDir& GetInstance();

        static void SetWorkDir(const std::string &path);

        static void SetProjectDir(const std::string &path);

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
        std::string m_workdir;
        std::string m_project_dir;
        std::vector<std::string> models_list;
};

#endif // FILES_INCLUDED_H
