#pragma once
#include "../socket.hpp"
#include "arg.hpp"
#include "worker.hpp"

namespace xrun {
class Server {
  private:
    std::vector<Job>         jobs;
    std::vector<WorkerGroup> worker_groups;
    FileDescriptor           epfd;

    auto assign_jobs(WorkerGroup* target = nullptr) -> void;
    auto parse_recieved(const std::vector<uint8_t>& data) -> std::vector<Job>;
    auto handle_command(const std::string& input) -> bool;
    auto add_worker_group(const std::string& address) -> WorkerGroup*;
    auto add_epoll_handle(int fd, const void* data) -> void;
    auto update_epoll_handle_data() const -> void;

  public:
    auto run(const Args& args) -> void;
};
} // namespace xrun
