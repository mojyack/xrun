#include <cstring>

#include "../byte.hpp"
#include "../error.hpp"
#include "../protocol.hpp"
#include "process.hpp"
#include "worker.hpp"

namespace xrun {
namespace {
auto build_error_packet(const std::string& cmd, const process::CloseResult& result) -> std::vector<uint8_t> {
    auto r = std::vector<uint8_t>();

    const auto len = sizeof(WorkerGroupMessage) + sizeof(size_t) + cmd.size() + 1 + 1 + sizeof(size_t) + result.out.size() + sizeof(size_t) + result.err.size();
    r.reserve(len);

    append_bytes(r, WorkerGroupMessage::ERROR);
    append_bytes(r, cmd.size());
    append_bytes(r, cmd.data(), cmd.size());
    append_bytes(r, static_cast<char>(result.status.reason == process::ExitReason::Exit ? 1 : 0));
    append_bytes(r, static_cast<char>(result.status.code));
    append_bytes(r, result.out.size());
    append_bytes(r, result.out.data(), result.out.size());
    append_bytes(r, result.err.size());
    append_bytes(r, result.err.data(), result.err.size());

    return r;
}
} // namespace
auto Worker::proc(const EventFileDescriptor& done, SendPacketFunc send_packet) -> void {
    while(true) {
        busy.store(false);
        auto received = channel.read();
        busy.store(true);

        if(std::holds_alternative<Job>(received)) {
            const auto job         = std::move(std::get<Job>(received));
            auto       proc        = process::Process();
            const auto open_result = proc.open("/usr/bin/zsh", job.command.data(), job.cwd.data(), {false, true, true});
            if(open_result.message != nullptr) {
                panic(stderr, "Failed to open process(%d)\n", open_result.error_num);
            }

            const auto close_result = proc.close();
            if((close_result.status.reason == process::ExitReason::Exit && close_result.status.code != 0) || close_result.status.reason == process::ExitReason::Signal) {
                send_packet(build_error_packet(job.command, close_result));
            }
            done.notify();
        } else {
            const auto message = std::get<Message>(received);
            switch(message) {
            case Message::KILL:
                return;
                break;
            }
        }
    }
}
auto Worker::launch(const EventFileDescriptor& done, SendPacketFunc send_packet) -> void {
    thread = std::thread(&Worker::proc, this, std::ref(done), send_packet);
}
auto Worker::assign_job(Job job) -> void {
    channel.write(job);
}
auto Worker::send_message(Message message) -> void {
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
} // namespace xrun
