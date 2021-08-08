#include <ifaddrs.h>
#include <netinet/ip.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "error.hpp"
#include "socket.hpp"

namespace xrun {
namespace {
auto create_socket(const int type) -> OpenSocketResult {
    const auto fd = socket(type, SOCK_STREAM, 0);
    if(fd < 0) {
        return {-1, "socket() failed", errno};
    } else {
        return {fd};
    }
}
auto create_local_socket() -> OpenSocketResult {
    return create_socket(AF_UNIX);
}
auto create_tcp_socket() -> OpenSocketResult {
    return create_socket(AF_INET);
}
auto create_sockaddr_un(const char* const path) -> sockaddr_un {
    auto addr = sockaddr_un();
    memset(&addr, 0, sizeof(sockaddr_un));
    addr.sun_family = AF_UNIX;

    // strcpy(addr.sun_path, path);
    // path[0] may be '\0'
    const auto len = strlen(&path[1]) + 1;
    memcpy(addr.sun_path, path, len);

    return addr;
}
} // namespace
auto open_local_server_socket(const char* const path) -> OpenSocketResult {
    auto result = create_local_socket();
    if(result.message != nullptr) {
        return result;
    }

    auto addr = create_sockaddr_un(path);

    if(bind(result.fd, (sockaddr*)&addr, sizeof(sockaddr_un)) < 0) {
        return {-1, "bind() failed", errno};
    }
    if(listen(result.fd, 2) < 0) {
        return {-1, "listen() failed", errno};
    } else {
        return result;
    }
}
auto open_local_client_socket(const char* const path) -> OpenSocketResult {
    auto result = create_local_socket();
    if(result.message != nullptr) {
        return result;
    }

    auto addr = create_sockaddr_un(path);

    if(connect(result.fd, (sockaddr*)&addr, sizeof(sockaddr_un)) < 0) {
        return {-1, "connect() failed", errno};
    } else {
        return result;
    }
}
auto open_tcp_server_socket(const std::array<uint16_t, 2> port, uint16_t* const selected) -> OpenSocketResult {
    auto result = create_tcp_socket();
    if(result.message != nullptr) {
        return result;
    }
    auto addr = sockaddr_in{.sin_family = AF_INET, .sin_addr = {INADDR_ANY}};
    for(auto p = port[0]; p <= port[1]; p += 1) {
        addr.sin_port = htons(p);
        if(bind(result.fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
            if(errno == EADDRINUSE) {
                continue;
            }
            return {-1, "bind() failed", errno};
        }
        break;
    }
    if(listen(result.fd, 1) < 0) {
        return {-1, "listen() failed", errno};
    } else {
        if(selected != nullptr) {
            *selected = ntohs(addr.sin_port);
        }
        return result;
    }
}
auto open_tcp_client_socket(const uint32_t address, const uint16_t port) -> OpenSocketResult {
    auto result = create_tcp_socket();
    if(result.message != nullptr) {
        return result;
    }
    auto addr = sockaddr_in{.sin_family = AF_INET, .sin_port = htons(port), .sin_addr = {address}};
    if(connect(result.fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
        return {-1, "connect() failed", errno};
    }
    return result;
}
auto get_self_address() -> std::vector<uint32_t> {
    ifaddrs* ifaddr;
    if(getifaddrs(&ifaddr) == -1) {
        panic("getifaddrs() failed");
    }

    auto r = std::vector<uint32_t>();
    for(auto ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if(ifa->ifa_addr == NULL) {
            continue;
        }

        if(ifa->ifa_addr->sa_family != AF_INET) {
            continue;
        }

        r.emplace_back(reinterpret_cast<sockaddr_in*>(ifa->ifa_addr)->sin_addr.s_addr);
    }
    freeifaddrs(ifaddr);
    return r;
}
Connection::Connection(const int fd, const uint32_t address) : connection(fd), address(address) {}
Connection::Connection(Connection&& o) : connection(o.connection), address(o.address){};
auto Connection::connect(const int fd) -> std::optional<Connection> {
    auto addr = sockaddr_in();
    memset(&addr, 0, sizeof(sockaddr_in));
    auto addrlen = static_cast<socklen_t>(sizeof(sockaddr_in));

    const auto client = accept(fd, (sockaddr*)&addr, &addrlen);
    if(client < 0) {
        return std::nullopt;
    }

    return Connection(client, addr.sin_family == AF_INET ? addr.sin_addr.s_addr : 0);
}
} // namespace xrun
