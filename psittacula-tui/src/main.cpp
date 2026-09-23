#include "app.h"
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
#ifdef _WIN32
#include <windows.h>
#include <codecvt>
#endif

using tui::App;
using console::TextOrigin;

// Limit for lines in a single query
const int kMaxLinesInAQuery = 1000;

// A simple console LLM chat program.
// Suports tools (see psittacula-tools sub-project).
// File manipulation tools are allowed to work with a sandbox only.
// To choose a sandbox use /project command.
// 
// The program 'workdir' directory structure (the default dir is created in a user home directory):
//  |-- %USER_HOME%/[Documents]/Psittacula
//    |-- models (txt files with models credentials)
//    |-- settings (txt settings files: rules)
//    |-- projects (files in the directory can be created / removed / modified by the agent - default sandbox)
//    |-- logs (log files directory)
//    |-- saves (dialogues dumps directory)
// 
// The workdir can be changed via command line value workdir (workdir=...)
// The projects directory can be changed via command line value project (project=...) / or command /project
//
// Example:
// 
// ./psittacula workdir=~/Documents/MyDirectory project=~/Documents/Projects/MyProject
//
// or use default parameters (no workdir / project arguments): 
// %USER_HOME%/[Documents]/Psittacula
// %USER_HOME%/[Documents]/Psittacula/projects
// 
int main(int argc, char **argv)
{
    App app;
    Formatter formatter;

    console::set_up_console(&app);

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
    
    console::write(get_logo_raw());
    console::write(formatter.Format("Actual workdir: %?\n", WorkingDir::GetInstance().GetWorkDir()));
    console::write(formatter.Format("Actual project: %?\n--------------\n", WorkingDir::GetInstance().GetProjectDir()));
    
    // Init chat commands: /help or /h for info about commands
    std::unique_ptr<ChatCommandDispatcher> cmd_dispatcher = std::make_unique<ChatCommandDispatcher>();
    init_commands(cmd_dispatcher.get(), &app);

    // AI API client
    // The client is created / updated in a CommandChangeModel ('model create' / 'model'), see below
    std::unique_ptr<AiClient> client;

    // Create at least one model, if there are no models to connect
    bool no_models = WorkingDir::GetInstance().GetModelsList().empty();
    if (no_models)
    {
        cmd_dispatcher->DispatchCommand("model", std::vector<std::string> {"create"}, client);
    }
    
    // Choose model and assign a client to the model
    cmd_dispatcher->DispatchCommand("model", std::vector<std::string> {}, client);

    app.SetClient(client);
    app.SetCommandDispatcher(cmd_dispatcher);
    app.Run();

    return 0;
}