#include <cstdio>

#include "workers.hpp"

const static auto HELP =
    R"(Usage: xworker [Options]
Options:
    -j --jobs N             Run N jobs in parallel
    -l --local              Startup for local server
    -r --replace A,B{,OPT}  Replace A in command or cwd with B
                            You can specify some options in third field
                            Options:
                                g  Replace all
                                c  Command only
                                w  Working directory only
    -h --help               Print this help
)";
int main(const int argc, const char* const argv[]) {
    const auto args = xrun::parse_args(argc, argv);
    if(args.help) {
        printf("%s\n", HELP);
        return 0;
    }
    xrun::WorkerGroup().run(args);
    return 0;
}
