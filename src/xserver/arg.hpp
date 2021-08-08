#pragma once
#include <string>
#include <vector>

namespace xrun {
struct Args {
    std::vector<std::string> remotes;
    bool                     help = false;
};
auto parse_args(int argc, const char* const argv[]) -> Args;
} // namespace xrun
