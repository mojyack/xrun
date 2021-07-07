#include "error.hpp"
#include "server-server.hpp"

namespace xrun::server {
auto server_main(const char* sock_path, int argc, const char* argv[]) -> int {
    try {
        auto s = server::Server(sock_path);
        s.run();
    } catch(const std::runtime_error& e) {
        panic(e.what());
    }
    return 0;
}
} // namespace xrun
