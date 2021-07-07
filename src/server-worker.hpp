#pragma once
#include "server-private.hpp"
#include "channel.hpp"
#include "thread.hpp"

namespace xrun::server {
class Worker {
  private:
    Channel<JobMessage> channel;
    std::thread         thread;
    SafeVar<bool>       busy = false;

    auto wait_for_message() -> JobMessage;
    auto try_pop_job(SafeVar<std::vector<Job>>& jobs) -> std::optional<std::pair<Job, size_t>>;
    auto proc(const int cpu_num, const int cpu_count, SafeVar<std::vector<Job>>& jobs, const int job_done_event) -> void;
  
  public:
    auto launch(const int cpu_num, const int cpu_count, SafeVar<std::vector<Job>>& jobs, const int job_done_event) -> void;
    auto send_message(const JobMessage message) -> void;
    auto is_busy() const -> bool;

    Worker() = default;
    ~Worker();
};
}
