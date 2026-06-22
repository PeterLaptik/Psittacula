#ifndef COMMAND_CHANGE_MODEL_INCLUDED_H
#define COMMAND_CHANGE_MODEL_INCLUDED_H

#include "chat_command.h"
#include "console_writer.h"
#include "format_util.h"
#include "working_dir.h"
#include "model.h"
#include <iostream>
#include <fstream>
#include <filesystem>

#ifdef _WIN32
#include <conio.h>
#else
#include <termios.h>
#include <unistd.h>

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

/// Changes or creates model to connect
class CommandChangeModel: public ChatCommand
{
    public:
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
            return "Change model in interractive mode. Does not clear current context. \n\r\t[ARGS]: [create] is to create model connection in an interactive mode and connect to a new model.";
        }

    private:
        Formatter formatter;

        // Select model from the list of found models in the working directory in interractive mode. 
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
                        DrawMenu();
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
            AiClient *created_client = model.GetClient();

            if(client.get() != nullptr)
                created_client->RestoreDialogueFrom(client->GetDialogueBody());

            client.reset(created_client);

            RestoreMainScreen();
        }

        void CreateModel(std::unique_ptr<AiClient> &client)
        {
            bool no_models = WorkingDir::GetInstance().GetModelsList().empty();

            ActivateAlternateScreen();

            if (no_models)
                console::write_line("No models found. Create at least one connection!\n\n", TextOrigin::error);

            console::write_line("============ Create new model connection ==================", TextOrigin::filesystem);

            std::string file_name;
            std::string model_name;
            std::string host;
            std::string api_key;
            std::string context_size_str;

            std::cout << "Enter file name for the connection: ";
            std::getline(std::cin, file_name);

            std::cout << "Enter model name (e.g., gpt-4o): ";
            std::getline(std::cin, model_name);

            std::cout << "Enter host URL (e.g., http://localhost:8080): ";
            std::getline(std::cin, host);

            std::cout << "Enter context size (optional, leave empty if not set): ";
            std::getline(std::cin, context_size_str);

            std::cout << "Enter API key (leave empty if not required): ";
            std::getline(std::cin, api_key);

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
            file << "# The value will be concatenate with '/chat/completions' for requests\n";
            file << "host=" << host << "\n\n";

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

            console::write_line("Configuration file '" + file_path + "' created successfully.\n", TextOrigin::filesystem);
        }

        // Main menu to choose a model
        void DrawMenu(const std::vector<std::string> &models, int index)
        {
            std::cout << "\x1b[2J\x1b[H"; // clear + home
            console::write_line("====================================================", TextOrigin::filesystem);
            console::write_line("============ Choose model to work ==================", TextOrigin::filesystem);
            console::write_line("====================================================", TextOrigin::filesystem);
            console::write_line(formatter.Format("Found models: %?", models.size()), TextOrigin::filesystem);
            console::write_line("Use Up/Down to choose, Enter to select.\n");

            for (int i = 0; i < models.size(); ++i)
            {
                if (i == index)
                    std::cout << "\x1b[7m"; // highlight

                std::cout << (i + 1) << " - " << models[i] << "\x1b[0m\n";
            }
        }
};

#endif // COMMAND_CHANGE_MODEL_INCLUDED_H