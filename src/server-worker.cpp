#include <sstream>

#include "process.hpp"
#include "server-worker.hpp"

namespace {
auto escape_argument(const char* arg) -> std::string {
    auto res = std::string();
    res += '\'';
    const char* c = arg;
    while(*c != '\0') {
        if(*c == '\'') {
            res += "\\\'";
        } else {
            res += *c;
        }
        c += 1;
    }
    res += '\'';
    return res;
}
} // namespace

namespace xrun::server {
auto Worker::wait_for_message() -> JobMessage {
    busy.store(false);
    const auto m = channel.read();
    busy.store(true);
    return m;
}
auto Worker::try_pop_job(SafeVar<std::vector<Job>>& jobs) -> std::optional<std::pair<Job, size_t>> {
    std::lock_guard<std::mutex> lock(jobs.mutex);
    if(jobs.data.size() >= 1) {
        const auto job = std::move(jobs.data[0]);
        jobs.data.erase(jobs.data.begin());
        return std::make_pair(job, jobs.data.size());
    }
    return std::nullopt;
}
auto Worker::proc(const int cpu_num, const int cpu_count, SafeVar<std::vector<Job>>& jobs, const int job_done_event) -> void {
    auto cpu_str     = std::vector<char>(cpu_count, '-');
    cpu_str[cpu_num] = '*';

    while(true) {
        auto       popped_job = std::optional<std::pair<Job, size_t>>();
        const auto message    = channel.try_read();
        if(message == std::nullopt) {
            popped_job = try_pop_job(jobs);
        }
        switch(message == std::nullopt ? (popped_job == std::nullopt ? wait_for_message() : JobMessage::Update) : message.value()) {
        case JobMessage::Kill:
            return;
        case JobMessage::Update: {
            if(popped_job == std::nullopt) {
                popped_job = try_pop_job(jobs);
            }
            if(popped_job != std::nullopt) {
                const auto& j = popped_job.value().first;

                std::stringstream command;
                const auto&       cmd = *j.get_command().get();
                command << cmd.command << " " << escape_argument(j.get_arg());
                auto       proc        = process::Process();
                const auto open_result = proc.open("/usr/bin/zsh", command.str().data(), cmd.cwd.data(), {false, true, true});
                if(open_result.message != nullptr) {
                    std::fprintf(stderr, "Failed to open process(%d)\n", open_result.error_num);
                    break;
                } else {
                    std::printf("%s %d [%zu] \"%s\" \"%s\"\n", cpu_str.data(), proc.get_pid(), popped_job.value().second, cmd.cwd.data(), j.get_arg());
                }

                const auto close_result = proc.close();
                switch(close_result.status.reason) {
                case process::ExitReason::Exit:
                    if(close_result.status.code != 0) {
                        std::fprintf(stderr, "Command \"%s\" returned exit code %d\n", command.str().data(), close_result.status.code);
                        std::fprintf(stderr, "==== stdout ====\n%s\n", close_result.out.data());
                        std::fprintf(stderr, "==== stderr ====\n%s\n", close_result.err.data());
                    }
                    break;
                case process::ExitReason::Signal:
                    std::fprintf(stderr, "Command \"%s\" terminated by signal %d\n", command.str().data(), close_result.status.code);
                    break;
                }

                int64_t v = 1;
                write(job_done_event, &v, sizeof(int64_t));
            }
        } break;
        default:
            break;
        }
    }
}
auto Worker::launch(const int cpu_num, const int cpu_count, SafeVar<std::vector<Job>>& jobs, const int job_done_event) -> void {
    thread = std::thread(&Worker::proc, this, cpu_num, cpu_count, std::ref(jobs), job_done_event);
}
auto Worker::send_message(const JobMessage message) -> void {
    channel.write(message);
}
auto Worker::is_busy() const -> bool {
    return busy.load();
}
Worker::~Worker() {
    if(thread.joinable()) {
        thread.join();
    }
}
} // namespace xrun::server
