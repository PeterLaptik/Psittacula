#ifndef CONSOLE_HISTORY_INCLUDED_H
#define CONSOLE_HISTORY_INCLUDED_H

#include <string>
#include <vector>

class ConsoleHistory
{
    public:
        ConsoleHistory() = default;
        virtual ~ConsoleHistory() = default;

        void AddText(const std::string &msg);

        void AddLine(const std::string &msg);

        void ClearAll();

        const std::vector<std::string>& GetHistory() const;

    private:
        std::string m_current_text;
        std::vector<std::string> m_history;
};

#endif // !CONSOLE_HISTORY_INCLUDED_H