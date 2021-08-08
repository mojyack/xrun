#include <cstring>

#include <getopt.h>

#include "../error.hpp"
#include "arg.hpp"

namespace xrun {
namespace {
auto find_comma(const char* p) -> const char* {
    while(true) {
        const auto c = std::strchr(p, ',');
        if(c == NULL) {
            return nullptr;
        }
        if(c == p) {
            return c;
        }
        if(*(c - 1) == '\\') {
            p = c + 1;
            continue;
        }
        return c;
    }
}
auto parse_replace(const char* const arg) -> ReplaceString {
    const auto p1 = find_comma(arg);
    if(p1 == nullptr) {
        panic("Invalid replace string");
    }
    auto       a  = std::string(arg, p1 - arg);
    const auto p2 = find_comma(p1 + 1);
    auto       b  = p2 != nullptr ? std::string(p1 + 1, p2 - (p1 + 1)) : std::string(p1 + 1);
    auto       r  = ReplaceString{.from = std::move(a), .to = std::move(b)};
    if(p2 == nullptr) {
        return r;
    }
    auto c = p2 + 1;
    while(*c != '\0') {
        switch(*c) {
        case 'g':
            r.global = true;
            break;
        case 'c':
            r.command_only = true;
            break;
        case 'w':
            r.cwd_only = true;
            break;
        default:
            panic("Unknown replace option");
            break;
        }
        c += 1;
    }
    return r;
}
} // namespace
auto parse_args(const int argc, const char* const argv[]) -> Args {
    int  local = 0, help = 0;
    auto result = Args();

    const auto   optstring  = "j:lr:h";
    const option longopts[] = {
        {"jobs", required_argument, 0, 'j'},
        {"local", no_argument, &local, 1},
        {"replace", required_argument, 0, 'r'},
        {"help", required_argument, &help, 1},
        {0, 0, 0, 0},
    };

    int longindex = 0;
    int c;
    while((c = getopt_long(argc, const_cast<char* const*>(argv), optstring, longopts, &longindex)) != -1) {
        switch(c) {
        case 'j':
            result.jobs = std::stoul(optarg);
            break;
        case 'l':
            local = 1;
            break;
        case 'r':
            result.replace.emplace_back(parse_replace(optarg));
            break;
        case 'h':
            help = 1;
            break;
        }
    }

    result.local = local != 0;
    result.help  = help != 0;

    return result;
}
} // namespace xrun
