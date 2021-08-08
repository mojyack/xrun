#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <unistd.h>

#include "fd.hpp"

namespace xrun {
struct OpenSocketResult {
    FileDescriptor fd;
    const char*    message = nullptr;
    int            error;
};
auto open_local_server_socket(const char* name) -> OpenSocketResult;
auto open_local_client_socket(const char* name) -> OpenSocketResult;
auto open_tcp_server_socket(const std::array<uint16_t, 2> port, uint16_t* selected = nullptr) -> OpenSocketResult;
auto open_tcp_client_socket(uint32_t address, uint16_t port) -> OpenSocketResult;

auto get_self_address() -> std::vector<uint32_t>;

class Connection {
  private:
    FileDescriptor connection;
    uint32_t       address;

    Connection(int fd, uint32_t address);

  public:
    auto get_fd() const -> const FileDescriptor& {
        return connection;
    }
    auto get_address() const -> uint32_t {
        return address;
    }
    Connection(Connection&& o);

    static auto connect(int fd) -> std::optional<Connection>;
};
} // namespace xrun
