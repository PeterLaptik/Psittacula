#ifndef TOOL_FILE_INCLUDED_H
#define TOOL_FILE_INCLUDED_H

#include "tool_base.h"

class ToolFile: public ToolBase
{
    protected:

        void CleanFilePathFromTrailingDots(std::string path) const
        {
            while (!path.empty() && path.back() == '.')
                path.pop_back();
        }
};

#endif // !TOOL_FILE_INCLUDED_H
