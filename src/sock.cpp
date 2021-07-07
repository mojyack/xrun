#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "sock.hpp"
#include "xrun.hpp"

namespace {
auto join_path(const char* a, const char* b) -> std::string {
    std::string str = a;
    if(str.back() == '/') {
        str += b;
    } else {
        str += '/';
        str += b;
    }
    return str;
}
auto read_size(int fd, char* data, const size_t size) -> bool {
    size_t len = 0;
    while(len < size) {
        const auto n = read(fd, data + len, size - len);
        if(n == -1) {
            return false;
        }
        len += n;
    }
    return true;
}
} // namespace
namespace xrun {
namespace {
auto create_socket() -> OpenSocketResult {
    const auto fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if(fd < 0) {
        return {errno, "socket() failed"};
    } else {
        return {fd};
    }
}
auto init_sockaddr(const char* path, sockaddr_un& addr) {
    memset(&addr, 0, sizeof(sockaddr_un));
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, path);
}
} // namespace
auto search_socket_path() -> std::optional<std::string> {
    const auto runtime_dir = getenv("XDG_RUNTIME_DIR");
    return runtime_dir != NULL ? std::optional(join_path(runtime_dir, "xrun.sock")) : std::nullopt;
}
auto open_server_socket(const char* path) -> OpenSocketResult {
    const auto result = create_socket();
    if(result.message != nullptr) {
        return result;
    }

    auto addr = sockaddr_un();
    init_sockaddr(path, addr);

    if(bind(result.fd, (sockaddr*)&addr, sizeof(sockaddr_un)) < 0) {
        return {errno, "bind() failed"};
    }
    if(listen(result.fd, 2) < 0) {
        return {errno, "listen() failed"};
    } else {
        return {result.fd};
    }
}
auto open_client_socket(const char* path) -> OpenSocketResult {
    const auto result = create_socket();
    if(result.message != nullptr) {
        return result;
    }

    auto addr = sockaddr_un();
    init_sockaddr(path, addr);

    if(connect(result.fd, (sockaddr*)&addr, sizeof(sockaddr_un)) < 0) {
        return {errno, "connect() failed"};
    } else {
        return {result.fd};
    }
}
auto socket_write(const int fd, const void* const data, const size_t size) -> int {
    size_t len = 0;
    while(len < size) {
        const auto n = write(fd, data, size);
        if(n == -1) {
            return errno;
        }
        len += n;
    }
    return 0;
}
auto accept_and_read(const int fd) -> SocketReadResult {
    sockaddr_un addr;
    memset(&addr, 0, sizeof(sockaddr_un));
    socklen_t addrlen = sizeof(sockaddr_un);

    const auto cfd = accept(fd, (sockaddr*)&addr, &addrlen);
    if(cfd < 0) {
        return {errno, "accept() failed"};
    }

    size_t size;
    if(!read_size(cfd, reinterpret_cast<char*>(&size), sizeof(size_t))) {
        return {errno, "read() failed"};
    }
    std::vector<char> data(size);
    if(!read_size(cfd, data.data(), size)) {
        return {errno, "read() failed"};
    }
    return {.data = data};
}
void close_server_socket(const int fd, const char* path) {
    close(fd);
    unlink(path);
}
void close_client_socket(const int fd) {
    close(fd);
}
} // namespace xrun
