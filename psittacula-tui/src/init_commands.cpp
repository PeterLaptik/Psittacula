#include "init_commands.h"
#include "command_change_model.h"
#include "command_tools.h"
#include "command_reasoning.h"
#include "command_undo.h"
#include "command_redo.h"

void init_commands(ChatCommandDispatcher &dsp)
{
    dsp.RegisterCommand("model", new CommandChangeModel());
    dsp.RegisterCommand("tools", new CommandTools());
    dsp.RegisterCommand("reasoning", new CommandReasoning());
    dsp.RegisterCommand("undo", new CommandUndo());
    dsp.RegisterCommand("redo", new CommandRedo());
}
