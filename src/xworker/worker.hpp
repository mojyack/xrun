#pragma once
#include <string>
#include <variant>
#include <functional>

#include "../fd.hpp"
#include "../thread.hpp"

namespace xrun {
struct Job {
    std::string cwd;
    std::string command;
};
enum class Message {
    KILL,
};
using WorkerMessage = std::variant<Job, Message>;

using SendPacketFunc = std::function<void(std::vector<uint8_t>&&)>;

class Worker {
  private:
    Channel<WorkerMessage> channel;
    std::thread            thread;
    SafeVar<bool>          busy = false;

    auto proc(const EventFileDescriptor& done, SendPacketFunc send_packet) -> void;

  public:
    auto launch(const EventFileDescriptor& done, SendPacketFunc send_packet) -> void;
    auto assign_job(Job job) -> void;
    auto send_message(Message message) -> void;
    auto is_busy() const -> bool;

    Worker() = default;
    ~Worker();
};
} // namespace xrun
