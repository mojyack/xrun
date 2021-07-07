#include <filesystem>

#include "client.hpp"
#include "error.hpp"
#include "server.hpp"
#include "sock.hpp"
#include "xrun.hpp"

namespace xrun {
auto run(int argc, const char* argv[]) -> int {
    const auto sock_path = search_socket_path();
    if(sock_path == std::nullopt) {
        panic("Cannot detect socket path.");
    }
    if(std::filesystem::is_socket(sock_path.value())) {
        return client::client_main(sock_path.value().data(), argc, argv);
    } else if(!std::filesystem::exists(sock_path.value())) {
        return server::server_main(sock_path.value().data(), argc, argv);
    } else {
        panic("Cannot create or open socket.");
        return -1;
    }
}
} // namespace xrun
