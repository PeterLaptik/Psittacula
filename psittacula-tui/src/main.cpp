#include "logo.h"
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

// Main dialogue loop
void main_loop(std::unique_ptr<AiClient> &client, ChatCommandDispatcher &cmd_dispatcher);

// Sends a text query to LLM server
void process_query(std::unique_ptr<AiClient> &client, const std::vector<std::string> &lines);

// Proceses a chat command (set project dir, set model, etc)
void process_command(std::unique_ptr<AiClient> &client, const std::string &command, ChatCommandDispatcher &cmd_dispatcher);


// Limit for lines in a single query
const int kMaxLinesInAQuery = 1000;


// A simple console LLM chat program.
// Uses tools (see psittacula-tools sub-prject).
// File manipulation tools are allowed to work with a sandbox only.
// To choose a sandbox use /project command.
// 
// The program 'workdir' directory structure (the default dir is created in a user home directory):
//  |-- %USER_HOME%/[Documents]/Psittacula
//    |-- models (txt files with models info: connection data)
//    |-- settings (txt settings files: sets of rules)
//    |-- projects (files in the directory can be created / removed / modified by the agent - default sandbox)
//    |-- logs (log and dump files directory)
// 
// The workdir can be changed via command line value workdir (workdir=...)
// The projects directory can be changed via command line value project (project=...) / or command /project
//
// Example:
// 
// ./psittacula workdir=~/Psittacula project=~/Documents/Projects/MyProject
//
// or use default parameters (no workdir / project arguments): 
// %USER_HOME%/[Documents]/Psittacula
// %USER_HOME%/[Documents]/Psittacula/projects
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

    // Working directory and project setup
    std::string workdir = param_reader.GetParam("workdir", "");
    std::string project = param_reader.GetParam("project", "");

    if (!workdir.empty())
    {
        console::write_line(formatter.Format("\nSet workdir = %?", workdir));
        WorkingDir::GetInstance().SetWorkDir(workdir);
    }

    if (!project.empty())
    {
        console::write_line(formatter.Format("Set project workspace = %?", project));
        WorkingDir::GetInstance().SetProjectDir(project);
    }

    console::write_line(formatter.Format("Actual workdir: %?", WorkingDir::GetInstance().GetWorkDir()));
    console::write_line(formatter.Format("Actual project: %?", WorkingDir::GetInstance().GetProjectDir()));

    // Init chat commands: /help or /h for info about commands
    ChatCommandDispatcher cmd_dispatcher;
    init_commands(cmd_dispatcher);

    // AI API client
    // The client is created / updated in a CommandChangeModel ('model create' / 'model'), see below
    std::unique_ptr<AiClient> client;

    // Create at least one model, if there are no models to connect
    bool no_models = WorkingDir::GetInstance().GetModelsList().empty();
    if (no_models)
    {
        cmd_dispatcher.DispatchCommand("model", std::vector<std::string> {"create"}, client);
    }
    
    // Choose model and assign a client to the model
    cmd_dispatcher.DispatchCommand("model", std::vector<std::string> {}, client);

    // Main program dialogue loop
    main_loop(client, cmd_dispatcher);
    
    // wait for press enter
    std::cin.get(); 

    return 0;
}

void main_loop(std::unique_ptr<AiClient> &client, ChatCommandDispatcher &cmd_dispatcher)
{
    std::vector<std::string> lines_acc; // lines of a current query

    console::write_line("\nDialogue:", TextOrigin::reasoning);
    console::write_splitter();
    console::write("\n>");

    std::string line;
    while (std::getline(std::cin, line)) {
        // Double enter -- process query
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
            console::write_line("\rToo many lines in the query! Max is " + std::to_string(kMaxLinesInAQuery), TextOrigin::error);
            process_query(client, lines_acc);
            console::write("\n>");
            continue;
        }

        // Exit from query input mode and clear accumulated lines
        if (line == "/q")
        {
            lines_acc.clear();
            console::write_line("\rQuery is cleared.\n", TextOrigin::machine);
            console::write(">");
            continue;
        }

        // Exit from application: /exit
        if (line == "/exit")
        {
            console::write("\r");
            console::write_line("\nBye...", TextOrigin::normal);
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

    cmd_dispatcher.DispatchCommand(cmd_name, cmd_args, client);
}
