#include <getopt.h>

#include "arg.hpp"

namespace xrun {
auto parse_args(const int argc, const char* const argv[]) -> Args {
    int  help = 0;
    auto result = Args();

    const auto   optstring  = "r:h";
    const option longopts[] = {
        {"remote", required_argument, 0, 'r'},
        {"help", required_argument, &help, 1},
        {0, 0, 0, 0},
    };

    int longindex = 0;
    int c;
    while((c = getopt_long(argc, const_cast<char* const*>(argv), optstring, longopts, &longindex)) != -1) {
        switch(c) {
        case 'r':
            result.remotes.emplace_back(optarg);
            break;
        case 'h':
            help = 1;
            break;
        }
    }

    result.help   = help != 0;

    return result;
}
} // namespace xrun
