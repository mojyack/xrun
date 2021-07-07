#pragma once
#include <vector>

#include "server-worker.hpp"

namespace xrun::server {
class Server {
  private:
    int                       sock_fd;
    int                       job_done_event;
    size_t                    cpu_count;
    const char* const         sock_path;
    std::vector<Worker>       workers;
    SafeVar<std::vector<Job>> jobs;

    auto parse_recieved(const std::vector<char>& data) -> std::vector<Job>;
    auto is_workers_busy() -> bool;
    auto shutdown() -> void;

  public:
    auto run() -> void;
    Server(const char* const sock_path);
};

} // namespace xrun::server
