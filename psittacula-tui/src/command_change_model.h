#ifndef COMMAND_CHANGE_MODEL_INCLUDED_H
#define COMMAND_CHANGE_MODEL_INCLUDED_H

#include "chat_command.h"
#include "console_writer.h"
#include "format_util.h"
#include "working_dir.h"
#include "model.h"
#include <iostream>
#include <fstream>

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

// Changes model to interract
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

        const std::string& Description() override
        {
            return m_description;
        }

    private:
        Formatter formatter;
        std::string m_description = "Change model in interractive mode. Does not clear current context. \n\r\t[ARGS]: [create] is to create model connection in an interactive mode and connect to a new model.";

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
                        draw();
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
            client.reset(model.GetClient());

            RestoreMainScreen();
        }

        void CreateModel(std::unique_ptr<AiClient> &client)
        {
            console::write_line("============ Create new model ==================", TextOrigin::filesystem);
            ActivateAlternateScreen();

            auto models_list = WorkingDir::GetInstance().GetModelsList();

            console::write_line(formatter.Format("Found models: %?", models_list.size()), TextOrigin::filesystem);
            console::write_line("Select model:\n");

            for (int i = 0; i < models_list.size(); ++i)
                console::write_line(formatter.Format("%? - %?", i + 1, models_list[i]));

            int model_choice = 0;
            std::cout << "\nChoose a model by number: ";

            while (!(std::cin >> model_choice))
            {
                std::cin.clear();
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                console::write_line("Invalid input. Enter a number.", TextOrigin::error);
                std::cout << "Choose a model by number: ";
            }

            if (model_choice < 1 || model_choice > models_list.size())
            {
                console::write_line("Invalid model choice", TextOrigin::error);
                RestoreMainScreen();
                return;
            }

            Model model = Model::FromFile(models_list[model_choice - 1]);
            client.reset(model.GetClient());
            RestoreMainScreen();
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
