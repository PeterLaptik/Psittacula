#ifndef COMMAND_CHANGE_MODEL_INCLUDED_H
#define COMMAND_CHANGE_MODEL_INCLUDED_H

#include "chat_command.h"
#include "command_clean_context.h"
#include "console_writer.h"
#include "format_util.h"
#include "working_dir.h"
#include "model.h"
#include "llm_connectivity_checker.h"
#include <iostream>
#include <fstream>
#include <filesystem>

#ifdef _WIN32
#include <conio.h>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#endif

/// Changes or creates model to connect
class CommandChangeModel: public ChatCommand
{
    public:
        using ChatCommand::ChatCommand;

        void Execute(std::unique_ptr<AiClient> &client, const std::vector<std::string> &args) override
        {
            if (args.empty())
            {
                ChooseModel(client);
                return;
            }

            std::string argument = args[0];
            std::transform(argument.begin(), argument.end(), argument.begin(),
                [](unsigned char c) { return std::tolower(c); });

            if (argument == "create")
            {
                CreateModel(client);
                return;
            }
            
            console::write(formatter.Format("Unknown argument: '?%'\n", argument), TextOrigin::error);
        }

        std::string Description() override
        {
            return "Change model in interactive mode. Does not clear current context. \n\r\t[ARG]: [create] is to create model connection in an interactive mode and connect to a new model.";
        }

    private:
        Formatter formatter;

#ifndef _WIN32
        int getch()
        {
            termios oldt, newt;
            tcgetattr(STDIN_FILENO, &oldt);
            newt = oldt;
            newt.c_lflag &= ~(ICANON | ECHO);
            tcsetattr(STDIN_FILENO, TCSANOW, &newt);

            int ch = getchar();

            tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
            return ch;
        }
#endif

        // Select model from the list of found models in the working directory in interactive mode. 
        void ChooseModel(std::unique_ptr<AiClient> &client)
        {
            ActivateAlternateScreen();

            auto models = WorkingDir::GetInstance().GetModelsList();
            if (models.empty())
            {
                console::write_line("No models found.", TextOrigin::error);
                RestoreMainScreen();
                return;
            }

            int index = 0; // currently highlighted item

            DrawMenu(models, index);

            // Read raw key input
            while (true)
            {
#ifdef _WIN32
                int c = _getch();
                if (c == 224) // arrow prefix
                {
                    int arrow = _getch();
                    if (arrow == 72 && index > 0) index--;                      // Up
                    else if (arrow == 80 && index < models.size() - 1) index++; // Down
                    DrawMenu(models, index);
                }
                else if (c == 13) // Enter
                {
                    break;
                }
#else
                int c = getch();
                if (c == '\x1b') // ESC
                {
                    int c1 = getch();
                    if (c1 == '[')
                    {
                        int c2 = getch();
                        if (c2 == 'A' && index > 0) index--;                        // Up
                        else if (c2 == 'B' && index < models.size() - 1) index++;   // Down
                        DrawMenu(models, index);
                    }
                }
                else if (c == '\n' || c == '\r')
                {
                    break;
                }
#endif
            }

            // Load selected model
            Model model = Model::FromFile(models[index]);

            std::cout << formatter.Format("Checking connectivity with '%?' ...", model.GetName());

            LlmConnectivityChecker checker(model.GetHost(), model.GetApiKey());
            LlmConnectivityState state = checker.Check();

            RestoreMainScreen();

            console::write_line(state.summary + "\n ", state.reachable ? TextOrigin::tools : TextOrigin::error);

            if (!state.reachable)
            {
                for (auto &probe : state.probes)
                {
                    std::string info = probe.details.empty() ? probe.error : probe.details;
                    console::write_line(formatter.Format("  %?: %? (%?ms)", probe.name, info, probe.latency_ms), TextOrigin::error);
                }

                m_app->RefreshStatus("\033[31mCannot connect to the selected model.\033[31m");
                return;
            }
            else
            {
                // Write a short model info to the status line
                std::string context_size_info = model.GetContextSize() > 0
                    ? std::to_string(model.GetContextSize())
                    : "n/a";

                std::string model_info = formatter.Format(
                    "Model: %? | Host: %? | Context: %?",
                    model.GetName(), model.GetHost(), context_size_info);

                m_app->RefreshStatus(model_info);
            }

            std::unique_ptr<AiClient> created_client = model.GetClient();

            if(client.get() != nullptr)
            { 
                std::string full_dialogue = client->GetDialogueBody();

                CommandCleanContext cmd_clean{ m_app };
                cmd_clean.Execute(client, {});

                created_client->RestoreDialogueFrom(full_dialogue);
            }

            client.reset(created_client.release());
        }

        void CreateModel(std::unique_ptr<AiClient> &client)
        {
            bool no_models = WorkingDir::GetInstance().GetModelsList().empty();

            ActivateAlternateScreen();

            if (no_models)
                console::write_line("No models found. Create at least one connection!\n\n", TextOrigin::error);

            console::write_line("============ Create new model connection ==================", TextOrigin::tools);

            std::string file_name;
            std::string model_name;
            std::string host;
            std::string api_key;
            std::string context_size_str;
            std::string chat_endpoint;

            {
                console::LineInputScope line_input;

                std::cout << "Enter file name for the connection: ";
                std::getline(std::cin, file_name);

                std::cout << "\nEnter model name (e.g., gpt-4o): ";
                std::getline(std::cin, model_name);

                std::cout << "\nEnter host URL (e.g. http://localhost:8080): ";
                std::getline(std::cin, host);

                std::cout << "\nEnter chat completion endpoint (leave empty for default '/v1/chat/completions'): ";
                std::getline(std::cin, chat_endpoint);

                std::cout << "\nEnter context size (leave empty for llama.cpp): ";
                std::getline(std::cin, context_size_str);

                std::cout << "\nEnter API key (leave empty if not required): ";
                std::getline(std::cin, api_key);
            }

            std::string models_dir = WorkingDir::GetInstance().GetModelsDir();

            std::string file_path = models_dir;
            file_path += std::filesystem::path::preferred_separator;
            file_path += file_name;
            file_path += ".txt";

            std::ofstream file(file_path);
            if (!file.is_open())
            {
                std::cerr << "Error: Could not open file for writing\n";
                return;
            }

            file << "# Model name\n";
            file << "name=" << model_name << "\n\n";

            file << "# Host\n";
            file << "# The value will be concatenated with the chat completion endpoint for requests\n";
            file << "host=" << host << "\n\n";

            file << "# Chat completion endpoint (optional)\n";
            if (!chat_endpoint.empty())
                file << "chat_endpoint=" << chat_endpoint << "\n\n";
            else
                file << "# chat_endpoint=/v1/chat/completions\n\n";

            file << "# Context size: maximum context length for the model (optional)\n";
            if (!context_size_str.empty())
                file << "context_size=" << context_size_str << "\n";
            else
                file << "# context_size=\n";

            file << "# Bearing key: add if necessary\n";
            if (!api_key.empty())
                file << "api_key=" << api_key << "\n";
            else
                file << "# api_key=\n";

            file.close();

            WorkingDir::GetInstance().UpdateModels();

            RestoreMainScreen();

            console::write_line("Configuration file '" + file_path + "' created successfully.\n", TextOrigin::tools);
        }

        int TerminalWidth() const
        {
#ifdef _WIN32
            HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);
            CONSOLE_SCREEN_BUFFER_INFO info;
            if (console != INVALID_HANDLE_VALUE && console != nullptr &&
                GetConsoleScreenBufferInfo(console, &info))
            {
                return info.srWindow.Right - info.srWindow.Left + 1;
            }
#else
            winsize ws = {};
            if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0)
                return ws.ws_col;
#endif
            return 80;
        }

        int TextWidth(const std::string &text) const
        {
            int width = 0;
            for (size_t i = 0; i < text.size();)
            {
                if (text[i] == '\033')
                {
                    size_t end = text.find('m', i + 1);
                    if (end == std::string::npos)
                        break;
                    i = end + 1;
                    continue;
                }

                size_t step = 1;
                unsigned char lead = static_cast<unsigned char>(text[i]);
                if ((lead & 0xE0) == 0xC0)
                    step = 2;
                else if ((lead & 0xF0) == 0xE0)
                    step = 3;
                else if ((lead & 0xF8) == 0xF0)
                    step = 4;
                if (i + step > text.size())
                    break;

                i += step;
                ++width;
            }
            return width;
        }

        void WriteCentered(const std::string &text, TextOrigin origin = TextOrigin::normal)
        {
            int pad = (TerminalWidth() - TextWidth(text)) / 2;
            if (pad < 0)
                pad = 0;
            std::cout << std::string(static_cast<size_t>(pad), ' ');
            console::write_line(text, origin);
        }

        // Main menu to choose a model
        void DrawMenu(const std::vector<std::string> &models, int index)
        {
            std::cout << "\x1b[2J\x1b[H"; // clear + home
            WriteCentered("====================================================", TextOrigin::tools);
            WriteCentered("============ Choose model to work ==================", TextOrigin::tools);
            WriteCentered("====================================================", TextOrigin::tools);
            WriteCentered(formatter.Format("Found models: %?", models.size()), TextOrigin::tools);
            WriteCentered("Use Up/Down to choose, Enter to select.\n");

            for (int i = 0; i < models.size(); ++i)
            {
                std::string item = std::to_string(i + 1) + " - " + models[i];

                int pad = (TerminalWidth() - TextWidth(item)) / 2;
                if (pad < 0)
                    pad = 0;

                std::cout << std::string(static_cast<size_t>(pad), ' ');
                if (i == index)
                    std::cout << "\x1b[7m"; // highlight

                std::cout << item << "\x1b[0m\n";
            }
            std::cout.flush();
        }
};

#endif // COMMAND_CHANGE_MODEL_INCLUDED_H