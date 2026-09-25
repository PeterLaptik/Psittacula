#pragma once

#include <vector>
#include <string>
#include <tuple>

class FilesHolder {
    public:
        void AddFile(const std::string &filename, const std::string &path) {
            m_files.emplace_back(filename, path);
        }

        void Clear() {
            m_files.clear();
        }

        const std::vector<std::tuple<std::string, std::string>>& GetFiles() const {
            return m_files;
        }

        size_t Size() const {
            return m_files.size();
        }

    private:
        std::vector<std::tuple<std::string, std::string>> m_files;
};