#pragma once
#include <optional>
#include <string>
#include <vector>

#include <cstdlib>

namespace xrun {
auto search_socket_path() -> std::optional<std::string>;

struct OpenSocketResult {
    int         fd;
    const char* message = nullptr;
};

auto open_server_socket(const char* path) -> OpenSocketResult;
auto open_client_socket(const char* path) -> OpenSocketResult;

struct SocketReadResult {
    int               err;
    const char*       message = nullptr;
    std::vector<char> data;
};

auto accept_and_read(const int fd) -> SocketReadResult;
auto socket_write(const int fd, const void* const data, const size_t size) -> int;

void close_server_socket(const int fd, const char* path);
void close_client_socket(const int fd);
} // namespace xrun
