#pragma once
#include <cstdint>
#include <vector>

#include "../socket.hpp"
#include "../thread.hpp"
#include "arg.hpp"

namespace xrun {
class WorkerGroup {
  private:
    std::optional<Connection>                  connection;
    SafeVar<std::vector<std::vector<uint8_t>>> packets;
    EventFileDescriptor                        packets_update;

    auto send_packet(std::vector<uint8_t>&& packet) -> void;

  public:
    auto run(const Args& args) -> void;
};
} // namespace xrun
