#include <cstring>
#include <filesystem>

#include "../byte.hpp"
#include "../error.hpp"
#include "../protocol.hpp"
#include "../socket.hpp"

namespace xrun {
namespace {
auto build_stream(const int argc, const char* const argv[]) -> std::vector<uint8_t> {
    auto res = std::vector<uint8_t>();
    append_bytes(res, ClientChunkType::COMMAND);
    const auto cwd = std::filesystem::current_path();
    append_bytes(res, cwd.c_str(), std::strlen(cwd.c_str()) + 1);
    append_bytes(res, argv[1], std::strlen(argv[1]) + 1);

    for(auto i = 2; i < argc; i += 1) {
        append_bytes(res, ClientChunkType::ARGUMENT);
        append_bytes(res, argv[i], std::strlen(argv[i]) + 1);
    }

    return res;
}
} // namespace
auto run(const int argc, const char* const argv[]) -> void {
    if(argc <= 1) {
        panic("Too few arguments");
    }
    auto fd = FileDescriptor(-1);
    if(auto r = open_local_client_socket("\0xrun"); r.message != nullptr) {
        panic("Failed to connect to xserver: ", r.message);
    } else {
        fd = r.fd;
    }
    const auto   data = xrun::build_stream(argc, argv);
    const size_t size = data.size();
    if(!fd.write(size) || !fd.write(data.data(), size)) {
        panic("Failed to write stream: ", errno);
    }
}
} // namespace xrun

auto main(const int argc, const char* const argv[]) -> int {
    xrun::run(argc, argv);
    return 0;
} // namespace xrun
