#include "console_history.h"
#include "console_writer.h"

using console::TextOrigin;

void ConsoleHistory::AddText(const std::string &msg)
{
    if (msg == ">")
        return; // ignore input char

    m_current_text += msg;
}

void ConsoleHistory::AddLine(const std::string &msg)
{
    if (msg == ">")
    {
        m_current_text.clear(); // ignore input char
        return;
    }

    m_current_text += msg;
    m_history.push_back(m_current_text);
    m_current_text.clear();
}

const std::vector<std::string> &ConsoleHistory::GetHistory() const
{
    return m_history;
}

void ConsoleHistory::ClearAll()
{
    m_history.clear();
}
