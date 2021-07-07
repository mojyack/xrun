#pragma once
#include <cstdio>
#include <thread>

#include <unistd.h>

namespace process {
struct OpenResult;
struct CloseResult;
struct ExitStatus;
class Process {
  private:
    pid_t       pid;
    std::FILE*  pipes[3] = {nullptr};
    std::thread output_collector;
    std::string outputs[2];
    int         output_notify;
    void collect_outputs();

  public:
    auto open(const char* shell, const char* command, const char* working_dir = nullptr, const std::array<bool, 3> open_pipe = {}) -> OpenResult;
    auto close() -> CloseResult;
    auto get_pid() const -> pid_t;

    Process() = default;
};

struct OpenResult {
    const char* message = nullptr;
    int         error_num;
};

enum ExitReason {
    Exit,
    Signal,
};

struct ExitStatus {
    ExitReason reason;
    int        code;
};

struct CloseResult {
    ExitStatus status;
    std::string out;
    std::string err;
};
} // namespace process
