#include <cstring>
#include <filesystem>

#include "client.hpp"
#include "error.hpp"
#include "protocol.hpp"
#include "sock.hpp"

namespace {
void append_bytes(std::vector<char>& data, const void* const copy, const size_t len) {
    const auto prev_size = data.size();
    data.resize(prev_size + len);
    std::memcpy(&data[prev_size], copy, len);
}
template <typename T>
void append_bytes(std::vector<char>& data, const T copy) {
    append_bytes(data, &copy, sizeof(copy));
}
} // namespace
namespace xrun::client {
namespace {
auto build_stream(const int argc, const char* argv[]) -> std::vector<char> {
    auto res = std::vector<char>();
    append_bytes(res, ChunkType::Command);
    const auto cwd = std::filesystem::current_path();
    append_bytes(res, cwd.c_str(), std::strlen(cwd.c_str()) + 1);
    append_bytes(res, argv[1], std::strlen(argv[1]) + 1);

    for(auto i = 2; i < argc; i += 1) {
        append_bytes(res, ChunkType::Argument);
        append_bytes(res, argv[i], std::strlen(argv[i]) + 1);
    }

    return res;
}
} // namespace
auto client_main(const char* sock_path, int argc, const char* argv[]) -> int {
    if(argc <= 2) {
        panic("Too few arguments");
    }

    const auto open_socket_result = open_client_socket(sock_path);
    if(open_socket_result.message != nullptr) {
        error_message << "Failed to open socket:" << open_socket_result.message << "(" << open_socket_result.fd << ")";
        panic();
    }

    const auto   data = build_stream(argc, argv);
    const size_t size = data.size();
    if(const auto ret = socket_write(open_socket_result.fd, &size, sizeof(size_t)) != 0) {
        error_message << "Failed to write stream size: " << ret;
        panic();
    }
    if(const auto ret = socket_write(open_socket_result.fd, data.data(), data.size()) != 0) {
        error_message << "Failed to write stream data: " << ret;
        panic();
    }

    close_client_socket(open_socket_result.fd);
    
    return 0;
}
} // namespace xrun
