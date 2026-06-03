#include "param_reader.h"
#include "working_dir.h"
#include "console_writer.h"
#include "format_util.h"
#include "init_commands.h"
#include <iostream>
#include <vector>
#include <memory>
#include <algorithm>

#ifndef PSITTACULA_APP_VERSION
#define PSITTACULA_APP_VERSION "unknown"
#endif

using console::TextOrigin;

void input_loop(std::unique_ptr<AiClient> &client, ChatCommandDispatcher &cmd_dispatcher);
void process_query(std::unique_ptr<AiClient> &client, const std::vector<std::string> &lines);
void process_command(std::unique_ptr<AiClient> &client, const std::string &command, ChatCommandDispatcher &cmd_dispatcher);
std::string get_logo();

// Limit for lines in a sinagle query
const int kMaxLinesInAQuery = 100;

//
// TODO brief about the program
// 
// The program 'workdir' directory structure:
//  |-- %USER_HOME%/[Documents]/Psittacula
//    |-- models (txt files with models info)
//    |-- settings (txt settings files)
//    |-- projects (files in the directorycan be created / removed / modified by the agent - default project dir)
// 
// The workdir can be changed via command line value workdir (workdir=...)
// The projects directory can be changed via command line value project (project=...)
//
// Example:
// 
// ./psittacula workdir=~/Psittacula project=~/Documents/Projects/MyProject
//
// or use default parameters (no workdir / project arguments)
// 
//
int main(int argc, char **argv)
{
    Formatter formatter;

    // Windows console setup
    console::set_up_console();

    // Logo + info
    std::cout << get_logo() << std::endl;

    // Parameters and flags from command line arguments
    ParamReader param_reader(argv, argc);
    param_reader.ReadParams();

    // Working directory
    std::string workdir = param_reader.GetParam("workdir", "");
    std::string project = param_reader.GetParam("project", "");

    console::write_line(formatter.Format("\nSet workdir = %?", workdir));
    console::write_line(formatter.Format("Set project workspace = %?", project));

    // Create and check working directory, default project directory, find models
    if (!workdir.empty())
        WorkingDir::GetInstance().SetWorkDir(workdir);

    if (!project.empty())
        WorkingDir::GetInstance().SetProjectDir(project);

    console::write_line(formatter.Format("Actual workdir: %?", WorkingDir::GetInstance().GetWorkDir()));
    console::write_line(formatter.Format("Actual project: %?", WorkingDir::GetInstance().GetProjectDir()));

    // Init chat commands: /help or /h for info about commands
    ChatCommandDispatcher cmd_dispatcher;
    init_commands(cmd_dispatcher);

    // AI API client
    // The client is created in a CommandChangeModel ('model create'), see below
    std::unique_ptr<AiClient> client;

    // Are there any models
    bool no_models = WorkingDir::GetInstance().GetModelsList().empty();
    if (no_models)
    {
        // Create at least one model to connect
        cmd_dispatcher.DispatchCommand("model", std::vector<std::string> {"create"}, client);
    }
    
    // Choose model and create AI client
    cmd_dispatcher.DispatchCommand("model", std::vector<std::string> {}, client);

    // Main program dialogue loop
    input_loop(client, cmd_dispatcher);
    
    std::cin.get(); // wait for press enter

    return 0;
}

void input_loop(std::unique_ptr<AiClient> &client, ChatCommandDispatcher &cmd_dispatcher)
{
    std::vector<std::string> lines_acc;
    console::write_line("\nDialogue:", TextOrigin::reasoning);
    console::write_splitter();
    console::write("\n>");

    std::string line;
    while (std::getline(std::cin, line)) {
        // Double enter - process query
        if (line.empty())
        {
            process_query(client, lines_acc);
            lines_acc.clear();
            console::write(">");
            continue;
        }

        // Lines limit for query
        if (lines_acc.size() >= kMaxLinesInAQuery)
        {
            console::write_line("\rToo many lines in query! Max is " + std::to_string(kMaxLinesInAQuery), TextOrigin::error);
            process_query(client, lines_acc);
            console::write("\n>");
            continue;
        }

        // Exit from query input mode and clear accumulated lines
        if (line == "/q")
        {
            console::write("\r");
            lines_acc.clear();
            console::write_line("Query cleared.\n", TextOrigin::machine);
            console::write(">");
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
            console::write("\n>");
            continue;
        }

        lines_acc.push_back(line);
    }
}

void process_query(std::unique_ptr<AiClient> &client, const std::vector<std::string> &lines)
{
    if (lines.empty())
        return;

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
    console::write("", TextOrigin::default);
    std::string logo = 
 R"(      ____________________________________________
      / __\ ___\//_ _/_ _// | /__\//  ///   / |
     / /_// /  // //  // //||//  //  ///   //||
    / ___/__ \// //  // //_||/  //  ///   //_||
   / /  ____\// //  // /___ |\_//__///__ /___ |
 __\/__/____//_//__//_//___||_/\___/____\/___||_
    )";

    logo += "Version: ";
    logo += PSITTACULA_APP_VERSION;
    logo += "\n    Written by Peter Laptik";
    logo += "\n\033[36m    Press /help or /h for information about commands\033[0m";
    return logo;
}
