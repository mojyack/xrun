#pragma once
#include <memory>

namespace xrun::server {
enum JobMessage {
    Kill,
    Update,
};

struct Command {
    std::string cwd;
    std::string command;
};

class Job {
  private:
    std::shared_ptr<Command> command;
    std::string              arg;

  public:
    auto set_command(Command* c) {
        command.reset(c);
    }
    auto set_command(const Job& o) {
        command = o.command;
    }
    auto get_command() const -> std::shared_ptr<Command> {
        return command;
    }
    auto set_arg(const char* a) {
        arg = a;
    }
    auto get_arg() const -> const char* {
        return arg.data();
    }
    auto has_arg() const {
        return !arg.empty();
    }
    Job(){};
};

}
