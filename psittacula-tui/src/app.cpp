#include "app.h"
#include "console_writer.h"
#include "chat_command_dispatcher.h"
#include "chunk_processor.h"
#include <algorithm>
#include <iostream>
#include <sstream>
#include <vector>

namespace {

    // Trims the line so it fits into the confirm frame
    std::string FitToWidth(const std::string &line, int width)
    {
        if (width <= 0)
            return "";
        if (static_cast<int>(line.length()) > width)
            return line.substr(0, static_cast<size_t>(width));
        return line;
    }

    // Prints margin spaces followed by the row body
    void PrintIndented(int margin, const std::string &line)
    {
        for (int i = 0; i < margin; i++)
            std::cout << ' ';
        std::cout << line << std::endl;
    }
} // namespace

tui::App::App()
    : m_exit_dialog(m_screen, m_keyboard)
{ }

tui::App::~App()
{
    // Resolve a possibly pending confirmation, so a worker waiting
    // for the dialog answer cannot block the shutdown
    {
        std::lock_guard<std::mutex> lock(m_confirm_mutex);
        if (m_confirm_active)
        {
            m_confirm_active = false;
            m_confirm_answered = true;
            m_confirm_result = false;
        }
    }
    m_confirm_cv.notify_all();

    // Never detach: if a query is still running at shutdown,
    // cancel it and wait for the worker to finish
    if (m_query_thread.joinable())
    {
        if (m_client)
            m_client->CancelRequest();
        m_query_thread.join();
    }
}

void tui::App::Run()
{
    m_screen.Show();
    MainLoop();
}

void tui::App::SendText(const std::string txt, TextOrigin origin)
{
    m_screen.PutText(txt, origin);
}

void tui::App::MoveSpinner()
{
    m_screen.MoveSpinner();
}

void tui::App::SetClient(std::unique_ptr<AiClient> &client)
{
    if (!client)
        return;

    m_client.reset(client.release());
}

void tui::App::SetCommandDispatcher(std::unique_ptr<ChatCommandDispatcher> &cmd_dispatcher)
{
    if (!cmd_dispatcher)
        return;

    m_cmd_dispatcher.reset(cmd_dispatcher.release());

    std::vector<std::string> cmd_list;
    cmd_list.push_back("/help");
    m_cmd_dispatcher->GetCommandList(cmd_list);
    m_screen.UpdateCommandsAutocompleteList(cmd_list);
}

void tui::App::SetShowReasoning(bool reasoning)
{
    m_screen.SetShowReasoning(reasoning);
}

bool tui::App::GetShowReasoning() const
{
    return m_screen.GetShowReasoning();
}

void tui::App::WriteLine(const std::string &message, TextOrigin origin)
{
    SendText(message + '\n', origin);
}

void tui::App::Write(const std::string &message, TextOrigin origin)
{
    SendText(message, origin);
}

bool tui::App::AskConfirm(const std::string message)
{
    // While a query streams, its worker thread is the caller: the UI loop
    // (which alone reads the keyboard) must render and answer the dialog
    if (m_query_running.load())
        return AskConfirmAsync(message);

    // Called on the UI thread: show the dialog directly
    return RunConfirmDialog(message, Keyboard::Keys::nothing);
}

bool tui::App::AskConfirmAsync(const std::string &message)
{
    {
        std::lock_guard<std::mutex> lock(m_confirm_mutex);
        if (m_confirm_active)
            return false; // the previous request has not been answered yet

        m_confirm_message = message;
        m_confirm_active = true;
        m_confirm_answered = false;
        m_confirm_result = false;
    }

    bool result = false;
    {
        std::unique_lock<std::mutex> lock(m_confirm_mutex);
        m_confirm_cv.wait(lock, [this] { return m_confirm_answered; });
        result = m_confirm_result;
    }
    return result;
}

bool tui::App::HandleConfirmRequest(int first_key)
{
    std::string message;
    {
        std::lock_guard<std::mutex> lock(m_confirm_mutex);
        if (!m_confirm_active)
            return false;
        message = m_confirm_message;
    }

    bool result = RunConfirmDialog(message, first_key);

    {
        std::lock_guard<std::mutex> lock(m_confirm_mutex);
        m_confirm_result = result;
        m_confirm_active = false;
        m_confirm_answered = true;
    }
    m_confirm_cv.notify_one();
    return true;
}

bool tui::App::RunConfirmDialog(const std::string &message, int first_key)
{
    int scr_width = 0, scr_height = 0;
    m_screen.GetSize(scr_width, scr_height);

    // The message can span lines: each line gets its own frame row
    std::vector<std::string> lines;
    {
        std::istringstream iss(message);
        std::string line;
        while (std::getline(iss, line))
            lines.push_back(line);
        if (lines.empty())
            lines.push_back(std::string());
    }

    const std::string title = "Confirm the action?";
    const std::string hint = "y - allow,  n/Esc - deny";

    int content_width = static_cast<int>(title.length());
    for (const std::string &line : lines)
        content_width = std::max(content_width, static_cast<int>(line.length()));

    content_width = std::min(content_width, std::max(1, scr_width - 6));

    const std::string border = "+" + std::string(static_cast<size_t>(content_width) + 2, '-') + "+";
    int box_width = content_width + 4;
    int box_margin = std::max(0, (scr_width - box_width) / 2);
    int hint_margin = std::max(0, (scr_width - static_cast<int>(hint.length())) / 2);

    int block_height = static_cast<int>(lines.size()) + 6; // borders, title, blank rows, hint
    int top_margin = std::max(0, (scr_height - block_height) / 2);

    std::cout << "\x1b[?1049h" << std::flush;
    m_screen.HideCursor(true);

    for (int i = 0; i < top_margin; i++)
        std::cout << std::endl;

    const std::string blank_content(static_cast<size_t>(content_width), ' ');
    const std::string blank_row(static_cast<size_t>(box_width) - 2, ' ');

    PrintIndented(box_margin, border);
    PrintIndented(box_margin, "| " + FitToWidth(title, content_width) + " |");
    PrintIndented(box_margin, "| " + blank_content + " |");
    for (const std::string &line : lines)
        PrintIndented(box_margin, "| " + FitToWidth(line, content_width) + " |");
    PrintIndented(box_margin, "|" + blank_row + "|");
    PrintIndented(box_margin, border);

    PrintIndented(hint_margin, hint);
    std::cout << std::flush;

    bool confirmed = false;
    int key = first_key;
    while (true)
    {
        if (key == Keyboard::Keys::nothing)
        {
            key = m_keyboard.ReadKey();
            if (key == Keyboard::Keys::nothing)
                continue;
        }

        if (key == 'y' || key == 'Y')
        {
            confirmed = true;
            break;
        }

        if (key == 'n' || key == 'N' || key == 27 || key == 3 || key == 4 ||
            key == Keyboard::Keys::eof)
        {
            confirmed = false;
            break;
        }

        key = Keyboard::Keys::nothing; // irrelevant keys are ignored
    }

    m_screen.HideCursor(false);
    std::cout << "\x1b[?1049l" << std::flush;
    return confirmed;
}

void tui::App::RefreshStatus(const std::string &status)
{
    m_screen.SetStatusLine(status);
}

void tui::App::Clear()
{
    m_screen.Clear();
}

void tui::App::Flush()
{
    // do nothing
}

void tui::App::MainLoop()
{
    int key;
    while (true)
    {
        ReapFinishedQuery();
        ShowQueryStatus();

        key = m_keyboard.ReadKey();

#ifdef _WIN32
        m_screen.HandleResize();
#else
        if (key == Keyboard::Keys::resize)
        {
            m_screen.HandleResize();
            continue;
        }
#endif

        if (key == Keyboard::Keys::nothing)
            continue;

        try
        {
            // While a query streams on the worker thread, only ESC is handled:
            // it interrupts the connection / chunk receiving
            if (m_query_running.load())
            {
                // The worker asks to confirm an action: render the dialog here,
                // the keyboard belongs to it until the user answers
                if (HandleConfirmRequest(key))
                    continue;

                if (key == 27 || key == 3 || key == 4)
                {
                    if (m_client)
                        m_client->CancelRequest();
                }
                continue;
            }

            Response response = m_screen.PutChar(key);

            if (response.result == ResponseResult::escape)
            {
                bool do_exit = m_exit_dialog.Confirm();
                if (do_exit)
                    return;
            }

            if (response.result == ResponseResult::enter)
            {
                m_screen.PutText(response.data + " \n ", TextOrigin::normal);
                ProcessQuery(response.data);
            }

            if (response.result == ResponseResult::command)
            {
                ProcessCommand(response.data);
            }

        }
        catch (...)
        {
            std::cerr << "App error" << std::endl;
        }
    }
}

void tui::App::ProcessCommand(const std::string &command)
{
    // Space delimited arguments list 
    std::vector<std::string> cmd_args;

    std::istringstream iss(command);
    std::string cmd_name;
    iss >> cmd_name;

    if (!cmd_name.empty() && cmd_name.front() == '/')
        cmd_name.erase(0, 1);

    std::string arg;
    while (iss >> arg)
        cmd_args.push_back(arg);

    m_cmd_dispatcher->DispatchCommand(cmd_name, cmd_args, m_client);
}

void tui::App::ProcessQuery(const std::string &query)
{
    if (query.empty())
        return;

    if (!m_client)
    {
        console::write_line("No model connected. Use /model first.", TextOrigin::error);
        return;
    }

    if (m_query_running.load())
    {
        console::write_line("A query is already running. Press ESC to interrupt it.", TextOrigin::error);
        return;
    }

    // Clean up a previously finished worker before starting a new one
    ReapFinishedQuery();

    // Run the blocking network call on a separate thread;
    // the UI loop keeps polling the keyboard for ESC to cancel
    m_query_status_shown = false;
    m_query_running.store(true);
    m_query_thread = std::thread(&App::ProcessQueryWorker, this, query);
}

void tui::App::ProcessQueryWorker(std::string query)
{
    std::string result = "ok";
    try
    {
        m_client->SendUserMessage(query);
    }
    catch (const std::exception &e)
    {
        result = std::string("error: ") + e.what();
    }
    catch (...)
    {
        result = "error: unknown failure";
    }

    {
        std::lock_guard<std::mutex> lock(m_query_mutex);
        m_query_result = result;
    }
    m_query_running.store(false);
}

void tui::App::ReapFinishedQuery()
{
    if (m_query_running.load() || !m_query_thread.joinable())
        return;

    m_query_thread.join();

    std::string result;
    {
        std::lock_guard<std::mutex> lock(m_query_mutex);
        result = m_query_result;
    }

    if (result != "ok")
        console::write_line(result, TextOrigin::error);
    else
        console::write_line(" ");
}

void tui::App::ShowQueryStatus()
{
    if (!m_query_running.load() || m_query_status_shown)
        return;

    m_query_status_shown = true;
    console::write_line("Working... (ESC to interrupt)", TextOrigin::reasoning);
}
