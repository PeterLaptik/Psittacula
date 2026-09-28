#include "json_dialogue.h"
#include "working_dir.h"
#include "console_writer.h"
#include <fstream>
#include <chrono>
#include <ctime>

void JsonDialogue::EnsureFilePath()
{
    if (!m_file_path.empty())
        return;

    std::string dir_to_save = WorkingDir::GetInstance().GetLogsDir();

    // Generate timestamp for a new session log file
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);

    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif

    char timestamp[32];
    std::strftime(timestamp, sizeof(timestamp), "%Y-%m-%d_%H-%M-%S", &tm);

    m_file_path = dir_to_save + "/dialogue_" + timestamp + ".txt";
}

void JsonDialogue::Write(const std::string &json_body)
{
    EnsureFilePath();

    std::ofstream out(m_file_path, std::ios::out | std::ios::trunc);
    if (!out)
    {
        console::write_line("Failed to autosave dialogue to: " + m_file_path, console::TextOrigin::error);
        return;
    }

    out << json_body;
    out.close();
}

std::string JsonDialogue::GetFilePath() const
{
    return m_file_path;
}

void JsonDialogue::Reset()
{
    m_file_path.clear();
}
