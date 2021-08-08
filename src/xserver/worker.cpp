#include <arpa/inet.h>

#include "../error.hpp"
#include "../protocol.hpp"
#include "worker.hpp"

namespace xrun {
auto WorkerGroup::get_address() const -> const uint32_t {
    return address;
}
auto WorkerGroup::get_fd() const -> const FileDescriptor& {
    return socket;
}
auto WorkerGroup::is_busy() const -> bool {
    return busy == workers;
}
auto WorkerGroup::decrement_busy() -> void {
    ASSERT(busy != 0, "Decrement non-busy workers")
    busy -= 1;
}
auto WorkerGroup::increment_busy() -> void {
    ASSERT(busy < workers, "Increment busy workers");
    busy += 1;
}
auto WorkerGroup::get_workers() const -> uint32_t {
    return workers;
}
auto WorkerGroup::get_busy() const -> uint32_t {
    return busy;
}
WorkerGroup::WorkerGroup(uint32_t address, FileDescriptor socket) : address(address), socket(socket) {
    do {
        if(!this->socket.write(WorkerGroupMessage::WORKERS)) {
            break;
        }
        if(const auto opt = this->socket.read<uint32_t>(); !opt.has_value()) {
            break;
        } else {
            workers = *opt;
        }
        warn("Conected to new workers: ", address == 0 ? "local" : inet_ntoa({address}));
        return;
    } while(0);
    panic("Failed to get worker numbers");
}
} // namespace xrun
