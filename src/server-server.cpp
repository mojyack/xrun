#include <cstring>
#include <sstream>
#include <stdexcept>
#include <thread>

#include <poll.h>
#include <sys/eventfd.h>

#include "protocol.hpp"
#include "server-server.hpp"
#include "sock.hpp"

#define THROW(m)         \
    std::stringstream e; \
    e << m;              \
    throw e.str();

#define SHUTDOWN \
    shutdown();  \
    return;

namespace {
template <typename T, std::size_t S>
std::size_t array_length(const T (&)[S]) {
    return S;
}
class ByteReader {
  private:
    const char* data;
    size_t      pos = 0;
    size_t      lim;

  public:
    template <class T>
    auto read() -> const T* {
        if(pos >= lim) {
            return nullptr;
        }
        pos += sizeof(T);
        return reinterpret_cast<const T*>(data + pos - sizeof(T));
    }
    auto read_until(const char c) -> const char* {
        const char* cptr = &data[pos];
        for(auto p = pos; p < lim; p += 1) {
            if(data[p] == c) {
                pos = p + 1;
                return cptr;
            }
        }
        return nullptr;
    }
    ByteReader(const std::vector<char>& data) : data(data.data()), lim(data.size()){};
    ByteReader(const char* data, const size_t limit) : data(data), lim(limit) {}
};
} // namespace

namespace xrun::server {
auto Server::parse_recieved(const std::vector<char>& data) -> std::vector<Job> {
    auto             reader = ByteReader(data);
    std::vector<Job> jobs;
    while(true) {
        const auto type = reader.read<ChunkType>();
        if(type == nullptr) {
            break;
        }
        switch(*type) {
        case ChunkType::Command: {
            jobs.emplace_back();
            auto c     = new Command;
            c->cwd     = reader.read_until('\0');
            c->command = reader.read_until('\0');
            jobs.back().set_command(c);
        } break;
        case ChunkType::Argument:
            if(jobs.back().has_arg()) {
                jobs.emplace_back();
                jobs.back().set_command(*(jobs.end() - 2));
            }
            jobs.back().set_arg(reader.read_until('\0'));
            break;
        }
    }
    return jobs;
}
auto Server::is_workers_busy() -> bool {
    for(auto& w : workers) {
        if(w.is_busy()) {
            return true;
        }
    }
    return false;
}
auto Server::shutdown() -> void {
    for(auto& w : workers) {
        w.send_message(JobMessage::Kill);
    }
    close_server_socket(sock_fd, sock_path);
    close(job_done_event);
}
auto Server::run() -> void {
    printf("Running as a server.\n");

    {
        const auto open_socket_result = open_server_socket(sock_path);
        if(open_socket_result.message != nullptr) {
            THROW("Failed to open socket: " << open_socket_result.message << "(" << open_socket_result.fd << ")")
        }
        sock_fd = open_socket_result.fd;
    }
    cpu_count      = std::thread::hardware_concurrency();
    workers        = std::vector<Worker>(cpu_count);
    job_done_event = eventfd(0, 0);

    for(unsigned int i = 0; i < cpu_count; i += 1) {
        workers[i].launch(i, cpu_count, jobs, job_done_event);
    }

    pollfd      fds[]    = {{.fd = sock_fd, .events = POLLIN}, {.fd = fileno(stdin), .events = POLLIN}, {.fd = job_done_event, .events = POLLIN}};
    auto&       sock_in  = fds[0];
    auto&       std_in   = fds[1];
    auto&       job_done = fds[2];
    std::string input;
    bool        exit_on_done = false;
    while(true) {
        if(poll(fds, array_length(fds), -1) == -1) {
            THROW("Failed at poll()");
        }
        if(sock_in.revents & POLLIN) {
            const auto res = accept_and_read(sock_fd);
            if(res.message != nullptr) {
                THROW("Failed to accept and read: " << res.message);
            }
            const auto received = parse_recieved(res.data);
            auto       job_num  = 0;
            {
                std::lock_guard<std::mutex> lock(jobs.mutex);
                std::copy(received.begin(), received.end(), std::back_inserter(jobs.data));
                job_num = jobs.data.size();
            }
            for(auto& w : workers) {
                if(job_num == 0) {
                    break;
                }
                if(!w.is_busy()) {
                    w.send_message(JobMessage::Update);
                    job_num -= 1;
                }
            }
        }
        if(std_in.revents & POLLIN) {
            constexpr auto BUF_LEN = 64;
            char           buf[BUF_LEN + 1];
            if(read(std_in.fd, buf, BUF_LEN) < 0) {
                THROW("read() failed.");
            }
            buf[BUF_LEN] = '\0';
            if(const auto c = std::strchr(buf, '\n'); c != NULL) {
                *c = '\0';
                input += buf;

                if(input == "q") {
                    SHUTDOWN
                } else if(input == "w") {
                    printf(is_workers_busy() ? "Process running\n" : "No process running.\n");
                } else if(input == "e") {
                    exit_on_done = !exit_on_done;
                    printf(exit_on_done ? "Exit on done.\n" : "Keep alive.\n");
                }

                input.clear();
            } else {
                input += buf;
            }
        }
        if(std_in.revents & POLLHUP) {
            THROW("stdin closed");
        }
        if(job_done.revents & POLLIN) {
            {
                int64_t v;
                read(job_done_event, &v, sizeof(int64_t));
            }
            if(exit_on_done) {
                bool jobs_left;
                {
                    std::lock_guard<std::mutex> lock(jobs.mutex);
                    jobs_left = jobs.data.size() != 0;
                }
                if(!jobs_left && !is_workers_busy()) {
                    SHUTDOWN
                }
            }
        }
    }
}
Server::Server(const char* const sock_path) : sock_path(sock_path) {}
} // namespace xrun::server
