#pragma once

namespace xrun::server {
auto server_main(const char* sock_path, int argc, const char* argv[]) -> int;
}
