#include "init_commands.h"
#include "command_change_model.h"
#include "command_tools.h"
#include "command_reasoning.h"
#include "command_undo.h"
#include "command_redo.h"
#include "command_dump.h"
#include "command_change_agent.h"
#include "command_change_project_dir.h"
#include "command_clean_context.h"
#include "command_restore.h"

#include <memory>

void init_commands(ChatCommandDispatcher &dsp)
{
    dsp.RegisterCommand("model", std::make_unique<CommandChangeModel>());
    dsp.RegisterCommand("tools", std::make_unique<CommandTools>());
    dsp.RegisterCommand("reasoning", std::make_unique<CommandReasoning>());
    dsp.RegisterCommand("undo", std::make_unique<CommandUndo>());
    dsp.RegisterCommand("redo", std::make_unique<CommandRedo>());
    dsp.RegisterCommand("dump", std::make_unique<CommandDump>());
    dsp.RegisterCommand("rules", std::make_unique<CommandChangeRules>());
    dsp.RegisterCommand("project", std::make_unique<CommandChangeProjectDir>());
    dsp.RegisterCommand("clear", std::make_unique<CommandCleanContext>());
    dsp.RegisterCommand("restore", std::make_unique<CommandRestore>());
}