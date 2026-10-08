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
    : m_exit_dialog(m_screen, m_keyboard), m_confirm_dialogue(m_screen)
{ }

tui::App::~App()
{
    if(m_client)
        m_client->CancelRequest();

    m_confirm_dialogue.Cancel();

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
    m_screen.PutText(message + '\n', origin);
}

void tui::App::Write(const std::string &message, TextOrigin origin)
{
    m_screen.PutText(message, origin);
}

bool tui::App::AskConfirm(const std::string &message)
{
    return m_confirm_dialogue.Confirm(message);
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
    // backward compatibility with legacy output API
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

        if (m_confirm_dialogue.IsShown())
        {
            m_confirm_dialogue.PutCharFromKeyboard(key);
            continue;
        }

        try
        {
            // While a query streams on the worker thread, only ESC is handled:
            // it interrupts the connection / chunk receiving
            if (m_query_running.load())
            {
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
                m_screen.PutText("---------------------------------------------------\n", TextOrigin::normal);
                m_screen.PutText(response.data + " \n ", TextOrigin::normal);
                ProcessQuery(response.data);
            }

            if (response.result == ResponseResult::command)
            {
                ProcessCommand(response.data);
            }

        }
        catch (const std::exception &e)
        {
            std::cerr << "App error: " << e.what() << std::endl;
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
