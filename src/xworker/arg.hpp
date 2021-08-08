#pragma once
#include <optional>
#include <string>
#include <vector>

namespace xrun {
struct ReplaceString {
    std::string from;
    std::string to;
    bool        global       = false;
    bool        command_only = false;
    bool        cwd_only     = false;
};

struct Args {
    std::optional<int>         jobs;
    bool                       local = false;
    std::vector<ReplaceString> replace;
    bool                       help = false;
};

auto parse_args(int argc, const char* const argv[]) -> Args;
} // namespace xrun
