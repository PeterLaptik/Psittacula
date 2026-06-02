#include "param_reader.h"
#include "working_dir.h"
#include "console_writer.h"
#include "format_util.h"
#include "init_commands.h"
#include <iostream>
#include <vector>
#include <memory>
#include <algorithm>

using console::TextOrigin;

void input_loop(std::unique_ptr<AiClient> &client, ChatCommandDispatcher &cmd_dispatcher);
void process_query(std::unique_ptr<AiClient> &client, const std::vector<std::string> &lines);
void process_command(std::unique_ptr<AiClient> &client, const std::string &command, ChatCommandDispatcher &cmd_dispatcher);
std::string get_logo();

const int kMaxLinesInAQuery = 100;

// The program workdir directory structure:
//  |-- %USER_HOME%/[Documents]/Psittacula
//    |-- models (txt files with models info)
//    |-- settings (txt settings files)
//    |-- projects (files in the directorycan be created / removed / modified by the agent)
// 
// The workdir can be changed via command line value workdir (workdir=...)
// The projects directory can be changed via command line value workspace (workspace=)
//
// Example:
// ./psittacula workdir=~/Psittacula workspace=~/Documents/Projects/MyProject
//
int main(int argc, char **argv)
{
    Formatter formatter;

    console::set_up_console();
    std::cout << get_logo() << std::endl;

    // Parameters and flags from command line arguments
    ParamReader param_reader(argv, argc);
    param_reader.ReadParams();

    // Working directory
    std::string workdir = param_reader.GetParam("workdir", "");
    std::string project = param_reader.GetParam("project", "");

    console::write_line(formatter.Format("Set workdir = %?", workdir));
    console::write_line(formatter.Format("Set project workspace = %?", project));

    // Create and check working directory, default project directory, find models
    if (!workdir.empty())
        WorkingDir::SetWorkDir(workdir);

    if (!workdir.empty())
        WorkingDir::SetProjectDir(project);

    console::write_line(formatter.Format("Actual workdir: %?", WorkingDir::GetInstance().GetWorkDir()));
    console::write_line(formatter.Format("Actual project: %?", WorkingDir::GetInstance().GetProjectDir()));

    // Init chat commands
    ChatCommandDispatcher cmd_dispatcher;
    init_commands(cmd_dispatcher);

    std::unique_ptr<AiClient> client;

    // Are there any models
    bool no_models = WorkingDir::GetInstance().GetModelsList().empty();
    if (no_models)
    {
        console::write_line("No models to choose. Create at least one model.");
        cmd_dispatcher.DispatchCommand("model", std::vector<std::string> {"create"}, client);
    }
    
    // Choose model and create AI client
    cmd_dispatcher.DispatchCommand("model", std::vector<std::string> {}, client);

    // Main program dialogue loop
    input_loop(client, cmd_dispatcher);

    // Waiting for enter press
    std::cin.get();

    return 0;
}

void input_loop(std::unique_ptr<AiClient> &client, ChatCommandDispatcher &cmd_dispatcher)
{
    std::vector<std::string> lines_acc;
    console::write_line("");

    std::string line;
    while (std::getline(std::cin, line)) {
       std::cout << "\033[0m" << ">";

        // Double enter - process query
        if (line.empty())
        {
            process_query(client, lines_acc);
            lines_acc.clear();
            continue;
        }

        // Lines limit for query
        if (lines_acc.size() >= kMaxLinesInAQuery)
        {
            console::write_line("\rToo many lines in query! Max is " + std::to_string(kMaxLinesInAQuery), TextOrigin::error);
            process_query(client, lines_acc);
            continue;
        }

        // Exit from query input mode and clear accumulated lines
        if (line == "/q")
        {
            console::write("\r");
            lines_acc.clear();
            console::write_line("Query cleared.\n", TextOrigin::machine);
            continue;
        }

        // Exit from application
        if (line == "/exit")
        {
            console::write("\r");
            console::write_line("\nBye...", TextOrigin::machine);
            break;
        }

        // Commands start with '/'
        if (!line.empty() && line[0] == '/')
        {
            process_command(client, line, cmd_dispatcher);
            console::write("\n");
            continue;
        }

        lines_acc.push_back(line);
    }
}

void process_query(std::unique_ptr<AiClient> &client, const std::vector<std::string> &lines)
{
    if (lines.empty())
        return;

    console::write("\r");
    console::flush();
    console::write_line("");

    std::ostringstream oss;
    for (const auto &line : lines) {
        oss << line << '\n';
    }

    client->SendUserMessage(oss.str());
    console::write_line("");
}

void process_command(std::unique_ptr<AiClient> &client, const std::string &command, ChatCommandDispatcher &cmd_dispatcher)
{
    std::vector<std::string> cmd_args;

    std::istringstream iss(command);
    std::string cmd_name;
    iss >> cmd_name;

    if (!cmd_name.empty() && cmd_name.front() == '/')
        cmd_name.erase(0, 1);

    std::string arg;
    while (iss >> arg)
        cmd_args.push_back(arg);

    cmd_dispatcher.DispatchCommand(cmd_name, cmd_args, client);
}

std::string get_logo()
{
    return R"("
      ___________________________________________
      / __\ ___\//_ _/_ _// | /__\//  ///   / |
     / /_// /  // //  // //||//  //  ///   //||
    / ___/__ \// //  // //_||/  //  ///   //_||
   / /  ____\// //  // /___ |\_//__///__ /___ |
 __\/__/____//_//__//_//___||_/\___/____\/___||_

    ")";
}
