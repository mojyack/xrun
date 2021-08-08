#include "arg.hpp"
#include "server.hpp"

const static auto HELP =
    R"(Usage: xserver
Options:
    -r --remote IP  Remote server (e.g.: 192.168.11.1)
                    You can add multiple servers by repeating this option.
    -h --help       Print this help
)";

int main(const int argc, const char* const argv[]) {
    const auto args = xrun::parse_args(argc, argv);
    if(args.help) {
        printf("%s\n", HELP);
        return 0;
    }
    xrun::Server().run(args);

    return 0;
}
