#include <arpa/inet.h>
#include <sys/epoll.h>

#include "../byte.hpp"
#include "../error.hpp"
#include "../protocol.hpp"
#include "worker.hpp"
#include "workers.hpp"

namespace xrun {
namespace {
auto parse_job_packet(const FileDescriptor& fd) -> Job {
    do {
        const auto opt = fd.read_sized();
        if(!opt.has_value()) {
            break;
        }
        const auto& data = *opt;
        auto        arr  = ByteReader(data);
        const auto  cwd  = reinterpret_cast<const char*>(arr.read_until('\0'));
        const auto  cmd  = reinterpret_cast<const char*>(arr.read_until('\0'));
        if(cwd == nullptr || cmd == nullptr) {
            break;
        }
        return {cwd, cmd};
    } while(0);
    panic("Failed to parse received job");
    return {};
}
auto replace_text(const std::string& str, const std::string& from, const std::string& to, const bool global) -> std::string {
    auto pos = std::string::size_type(0);
    auto r   = str;
    while(true) {
        const auto p = r.find(from, pos);
        if(p == std::string::npos) {
            return r;
        }
        const auto a = r.substr(0, p);
        const auto b = r.substr(p + from.size());
        r            = a + to + b;
        if(!global) {
            return r;
        }
        pos = p + 1;
    }
}
auto replace_job_text(const std::vector<ReplaceString>& replace, Job job) -> Job {
    // /home/mojyack/working/ /home/mojyack/remote/01-567/working
    for(const auto& r : replace) {
        if(!r.command_only) {
            job.cwd = replace_text(job.cwd, r.from, r.to, r.global);
        }
        if(!r.cwd_only) {
            job.command = replace_text(job.command, r.from, r.to, r.global);
        }
    }
    return job;
}
} // namespace
auto WorkerGroup::send_packet(std::vector<uint8_t>&& packet) -> void {
    const auto lock = packets.get_lock();
    packets->emplace_back(packet);
    packets_update.notify();
}
auto WorkerGroup::run(const Args& args) -> void {
    // open socket
    auto sock = FileDescriptor(-1);
    auto port = uint16_t();
    if(auto opt = args.local ? open_local_server_socket("\0xrun-local-worker") : open_tcp_server_socket({1024, 1034}, &port); opt.message != nullptr) {
        panic("Failed to open socket: ", opt.message);
    } else {
        if(args.local) {
            print("This worker is available for local server");
        } else {
            print("This worker is available for remote server");
            print("Address is:");
            for(const auto a : get_self_address()) {
                print("    ", inet_ntoa({a}), ":", port);
            }
        }
        sock = opt.fd;
    }

    // setup workers
    const auto workers_count = args.jobs.has_value() ? *args.jobs : std::thread::hardware_concurrency();
    auto       workers       = std::vector<Worker>(workers_count);
    const auto job_done      = EventFileDescriptor();
    for(auto& w : workers) {
        w.launch(job_done, std::bind(&WorkerGroup::send_packet, this, std::placeholders::_1));
    }

    // setup epoll
    const auto epfd = FileDescriptor(epoll_create(1));
    if(epfd < 0) {
        panic("epoll_create() failed: ", errno);
    }
    auto evset   = epoll_event();
    evset.events = EPOLLIN;
    {
        const int fds[] = {fileno(stdin), sock, job_done, packets_update};
        for(size_t i = 0; i < 4; i += 1) {
            evset.data.fd = fds[i];
            if(epoll_ctl(epfd, EPOLL_CTL_ADD, fds[i], &evset) < 0) {
                panic("epoll_ctl() failed: ", errno);
            }
        }
    }

    // main loop
    auto input = std::string();
    auto ev    = epoll_event();
    while(true) {
        if(epoll_wait(epfd, &ev, 1, -1) < 0) {
            panic("epoll_wait() failed: ", errno);
        }
        if(ev.data.fd == fileno(stdin)) {
            if(ev.events & EPOLLHUP || ev.events & EPOLLERR) {
                panic("stdin closed");
            } else if(ev.events & EPOLLIN) {
                constexpr auto BUF_LEN = 64;
                char           buf[BUF_LEN + 1];
                buf[BUF_LEN] = '\0';
                if(read(fileno(stdin), buf, BUF_LEN) < 0) {
                    panic("read() failed.");
                }
                if(const auto c = std::strchr(buf, '\n'); c != NULL) {
                    *c = '\0';
                    input += buf;
                    if(input == "q") {
                        break;
                    }
                    input.clear();
                } else {
                    input += buf;
                }
            }
        } else if(ev.data.fd == sock) {
            auto c = Connection::connect(sock);
            if(!c.has_value()) {
                panic("Failet to accept xserver");
            }
            const auto address = c->get_address();
            if(address == 0) {
                print("Connected to local server");
            } else {
                print("Connected to remote server ", inet_ntoa({address}));
            }
            connection.emplace(std::move(*c));
            evset.data.fd = connection->get_fd();
            epoll_ctl(epfd, EPOLL_CTL_ADD, evset.data.fd, &evset);
        } else if(connection.has_value() && ev.data.fd == connection->get_fd()) {
            if(ev.events & EPOLLHUP || ev.events & EPOLLERR) {
                print("Connection closed");
                epoll_ctl(epfd, EPOLL_CTL_DEL, ev.data.fd, NULL);
                connection.reset();
            } else if(ev.events & EPOLLIN) {
                const auto& fd   = connection->get_fd();
                const auto  type = fd.read<WorkerGroupMessage>();
                if(!type.has_value()) {
                    panic("Failed to read message from xserver");
                }
                switch(*type) {
                case WorkerGroupMessage::WORKERS:
                    fd.write(static_cast<uint32_t>(workers_count));
                    break;
                case WorkerGroupMessage::JOB: {
                    auto assigned = false;
                    for(auto& w : workers) {
                        if(!w.is_busy()) {
                            w.assign_job(replace_job_text(args.replace, parse_job_packet(fd)));
                            assigned = true;
                            break;
                        }
                    }
                    ASSERT(assigned, "Could not assign received job")
                } break;
                default:
                    panic("Received an invalid message ", static_cast<int>(*type));
                    break;
                }
            }
        } else if(ev.data.fd == job_done) {
            auto count = job_done.consume();
            if(connection.has_value()) {
                while(count > 0) {
                    connection->get_fd().write(WorkerGroupMessage::DONE);
                    count -= 1;
                }
            }
        } else if(ev.data.fd == packets_update) {
            packets_update.consume();
            if(connection.has_value()) {
                auto lock = packets.get_lock();
                for(const auto& p : *packets) {
                    connection->get_fd().write(p.data(), p.size());
                }
            }
        }
    }

    for(auto& w : workers) {
        w.send_message(Message::KILL);
    }
}
} // namespace xrun
