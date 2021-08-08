#pragma once
#include <memory>
#include <string>

#include "../fd.hpp"

namespace xrun {
struct Command {
    std::string cwd;
    std::string command;
};

class Job {
  private:
    std::shared_ptr<Command> command;
    std::string              arg;

  public:
    auto set_command(Command* c) -> void {
        command.reset(c);
    }
    auto set_command(const Job& o) -> void {
        command = o.command;
    }
    auto get_command() const -> std::shared_ptr<Command> {
        return command;
    }
    auto set_arg(const char* a) -> void {
        arg = a;
    }
    auto get_arg() const -> const char* {
        return arg.data();
    }
    auto has_arg() const -> bool {
        return !arg.empty();
    }
    Job(){};
};

class WorkerGroup {
  private:
    uint32_t       address;
    uint32_t       workers;
    uint32_t       busy = 0;
    FileDescriptor socket;

  public:
    auto get_address() const -> const uint32_t;
    auto get_fd() const -> const FileDescriptor&;
    auto is_busy() const -> bool;
    auto decrement_busy() -> void;
    auto increment_busy() -> void;
    auto get_workers() const -> uint32_t;
    auto get_busy() const -> uint32_t;
    WorkerGroup(uint32_t address, FileDescriptor socket);
};
} // namespace xrun
