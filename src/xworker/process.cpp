#include <cerrno>
#include <string>
#include <vector>

#include <poll.h>
#include <sys/eventfd.h>
#include <unistd.h>
#include <wait.h>

#include "process.hpp"

namespace process {
auto Process::open(const char* shell, const char* command, const char* working_dir, const std::array<bool, 3> open_pipe) -> OpenResult {
    int fds[3][2];
    for(auto i = 0; i < 3; i += 1) {
        if(!open_pipe[i]) {
            continue;
        }
        if(pipe(fds[i]) < 0) {
            for(auto j = 0; j < i; ++j) {
                if(!open_pipe[i]) {
                    continue;
                }
                ::close(fds[j][0]);
                ::close(fds[j][1]);
            }
            return {.message = "Failed to create pipe", .error_num = errno};
        }
    }
    const auto pid = vfork();
    if(pid < 0) {
        for(auto i = 0; i < 3; i += 1) {
            if(!open_pipe[i]) {
                continue;
            }
            ::close(fds[i][0]);
            ::close(fds[i][1]);
        }
        return {.message = "Failed to fork process", .error_num = errno};
    } else if(pid != 0) {
        this->pid = pid;
        for(auto i = 0; i < 3; ++i) {
            if(open_pipe[i]) {
                pipes[i] = fdopen(fds[i][i == 0 ? 1 : 0], i == 0 ? "w" : "r");
                ::close(fds[i][i == 0 ? 0 : 1]);
            }
        }
        output_notify    = eventfd(0, 0);
        output_collector = std::thread(&Process::collect_outputs, this);
        return {};
    } else {
        for(auto i = 0; i < 3; ++i) {
            dup2(fds[i][i == 0 ? 0 : 1], i);
            ::close(fds[i][0]);
            ::close(fds[i][1]);
        }
        if(working_dir != nullptr) {
            chdir(working_dir);
            if(chdir(working_dir) == -1) {
                _exit(-1);
            }
        }
        execl(shell, shell, "-l", "-c", command, NULL);
        _exit(0);
    }
}
auto Process::close(const bool force) -> CloseResult {
    if(force) {
        if(kill(pid, SIGKILL) == -1) {
            return {.message = "Failed to kill process"};
        }
    }

    int status;
    waitpid(pid, &status, 0);

    {
        int v = 1;
        write(output_notify, &v, sizeof(int));
    }
    output_collector.join();
    ::close(output_notify);

    for(auto i = 0; i < 3; ++i) {
        if(pipes[i] != nullptr) {
            fclose(pipes[i]);
        }
    }
    const bool exitted = WIFEXITED(status);
    return {{exitted ? ExitReason::Exit : ExitReason::Signal, exitted ? WEXITSTATUS(status) : WTERMSIG(status)}, std::move(outputs[0]), std::move(outputs[1])};
}
auto Process::collect_outputs() -> void {
    std::vector<pollfd> fds;
    if(pipes[1] != nullptr) {
        fds.emplace_back(pollfd{.fd = fileno(pipes[1]), .events = POLLIN});
    }
    if(pipes[2] != nullptr) {
        fds.emplace_back(pollfd{.fd = fileno(pipes[2]), .events = POLLIN});
    }
    fds.emplace_back(pollfd{.fd = output_notify, .events = POLLIN});
    while(true) {
        if(poll(fds.data(), fds.size(), -1) == -1) {
            // poll error
            return;
        }
        for(const auto& fd : fds) {
            if(fd.revents & POLLHUP) {
                // target process is exitted
                return;
            }
            if(!(fd.revents & POLLIN)) {
                continue;
            }
            if(fd.fd == fileno(pipes[1])) {
                char buf[256] = {};
                read(fds[0].fd, buf, 256);
                outputs[0] += buf;
            } else if(fd.fd == fileno(pipes[2])) {
                char buf[256] = {};
                read(fds[1].fd, buf, 256);
                outputs[1] += buf;
            } else if(fd.fd == output_notify) {
                return;
            }
        }
    }
}
auto Process::get_pid() const -> pid_t {
    return pid;
}
} // namespace process
