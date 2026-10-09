#include "test_framework.h"
#include "working_dir.h"

#include <algorithm>
#include <cctype>
#include <string>

namespace {
    std::string Sandbox()
    {
        return WorkingDir::GetInstance().GetProjectDir();
    }
}

PS_TEST(sandbox_root_itself_is_inside)
{
    const std::string root = Sandbox();
    PS_CHECK(!root.empty());
    PS_CHECK(WorkingDir::GetInstance().IsInWorkDir(root));
}

PS_TEST(path_inside_sandbox)
{
    const std::string root = Sandbox();
    PS_CHECK(WorkingDir::GetInstance().IsInWorkDir(root + "/src/main.cpp"));
    PS_CHECK(WorkingDir::GetInstance().IsInWorkDir(root + "/a/b/c/d.txt"));
}

PS_TEST(parent_of_sandbox_is_outside)
{
    const std::string root = Sandbox();
    const std::string parent = root.substr(0, root.find_last_of("/\\"));
    if (parent == root) // root already at the top: nothing to assert here
        return;

    PS_CHECK(!WorkingDir::GetInstance().IsInWorkDir(parent));
    PS_CHECK(!WorkingDir::GetInstance().IsInWorkDir(parent + "/other_project.txt"));
}

PS_TEST(empty_and_relative_paths_are_not_inside)
{
    // An empty or relative path cannot be attributed to the sandbox
    PS_CHECK(!WorkingDir::GetInstance().IsInWorkDir(""));
    PS_CHECK(!WorkingDir::GetInstance().IsInWorkDir("relative.txt"));
}

PS_TEST(traversal_attempt_is_outside)
{
    // A ".."-carrying path that resolves above the sandbox must not pass
    const std::string root = Sandbox();
    PS_CHECK(!WorkingDir::GetInstance().IsInWorkDir(root + "/../../outside.txt"));
}

PS_TEST(cyrillic_lead_bytes_survive_byte_wise_case_fold)
{
    // The invariant the Windows path normalizer relies on: the byte-wise
    // ::tolower fold must not alter UTF-8 lead (0xD0/0xD1) or continuation
    // bytes, so Cyrillic paths still compare equal after normalization
    const std::string cyrillic = "\xD0\x9F\xD1\x80\xD0\xB8"; // "При"
    std::string folded = cyrillic;
    std::transform(folded.begin(), folded.end(), folded.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    PS_CHECK_MSG(folded == cyrillic, "Cyrillic bytes must survive the fold used in IsInWorkDir");
}
