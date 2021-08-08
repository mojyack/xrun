#include <cstring>
#include <filesystem>
#include <optional>

#include <arpa/inet.h>
#include <sys/epoll.h>

#include "../byte.hpp"
#include "../error.hpp"
#include "../fd.hpp"
#include "../protocol.hpp"
#include "server.hpp"
#include "worker.hpp"

namespace {
auto parse_str_to_address(const std::string& str) -> std::optional<std::pair<uint32_t, uint16_t>> {
    const auto p = str.find(':');
    if(p == std::string::npos) {
        return std::nullopt;
    }
    const auto addr = str.substr(0, p);
    const auto port = str.substr(p + 1);

    auto number = in_addr();
    if(inet_aton(addr.data(), &number) != 1) {
        return std::nullopt;
    }

    auto port_n = uint16_t();
    try {
        port_n = std::stoul(port);
    } catch(const std::invalid_argument&) {
        return std::nullopt;
    }
    return std::make_pair(number.s_addr, port_n);
}
auto escape_argument(const char* const arg) -> std::string {
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
namespace xrun {
namespace {
auto build_job_packet(const std::string& cwd, const std::string& command, const std::string& arg) -> std::vector<uint8_t> {
    auto r = std::vector<uint8_t>();

    const auto escaped = escape_argument(arg.data());
    const auto header  = sizeof(WorkerGroupMessage) + sizeof(size_t);
    const auto data    = cwd.size() + 1 + command.size() + 1 + escaped.size() + 1;
    r.reserve(header + data);

    append_bytes(r, WorkerGroupMessage::JOB);
    append_bytes(r, static_cast<size_t>(data));
    append_bytes(r, cwd.data(), cwd.size() + 1);
    append_bytes(r, command.data(), command.size());
    append_bytes(r, ' ');
    append_bytes(r, escaped.data(), escaped.size() + 1);

    return r;
}
struct ErrorPacket {
    std::string command;
    std::string out;
    std::string err;
    bool        exitted;
    char        code;
};
auto parse_error_packet(const FileDescriptor& fd) -> ErrorPacket {
    do {
        auto       r        = ErrorPacket();
        const auto cmd_size = fd.read<size_t>();
        if(!cmd_size.has_value()) {
            break;
        }
        r.command.resize(*cmd_size);
        if(!fd.read(r.command.data(), *cmd_size)) {
            break;
        }

        const auto exitted = fd.read<uint8_t>();
        if(!exitted.has_value()) {
            break;
        }
        r.exitted       = *exitted != 0;
        const auto code = fd.read<uint8_t>();
        if(!code.has_value()) {
            break;
        }
        r.code = *code;

        const auto out_size = fd.read<size_t>();
        if(!out_size.has_value()) {
            break;
        }
        r.out.resize(*out_size);
        if(!fd.read(r.out.data(), *out_size)) {
            break;
        }

        const auto err_size = fd.read<size_t>();
        if(!err_size.has_value()) {
            break;
        }
        r.err.resize(*err_size);
        if(!fd.read(r.err.data(), *err_size)) {
            break;
        }
        return r;
    } while(0);
    panic("Failed to parse error packet");
    return {};
}
} // namespace
auto Server::assign_jobs(WorkerGroup* target) -> void {
    while(!jobs.empty()) {
        auto g = (WorkerGroup*)nullptr;
        if(target != nullptr) {
            if(!target->is_busy()) {
                g = target;
            }
        } else {
            for(auto& w : worker_groups) {
                if(!w.is_busy()) {
                    g = &w;
                    break;
                }
            }
        }
        if(g == nullptr) {
            // there is no free worker groups
            return;
        }
        while(!jobs.empty() && !g->is_busy()) {
            const auto& cwd    = jobs[0].get_command()->cwd;
            const auto& cmd    = jobs[0].get_command()->command;
            const auto& arg    = jobs[0].get_arg();
            const auto  packet = build_job_packet(cwd, cmd, arg);
            print("[", jobs.size() - 1, "] \"", cwd, "\" \"", arg, '"');
            jobs.erase(jobs.begin());
            const auto& fd = g->get_fd();
            if(!fd.write(packet.data(), packet.size())) {
                panic("Failed to send job packet");
            }
            g->increment_busy();
        }
    }
}
auto Server::parse_recieved(const std::vector<uint8_t>& data) -> std::vector<Job> {
    auto reader = ByteReader(data);
    auto jobs   = std::vector<Job>();
    while(true) {
        const auto type = reader.read<ClientChunkType>();
        if(type == nullptr) {
            break;
        }
        switch(*type) {
        case ClientChunkType::COMMAND: {
            jobs.emplace_back();
            auto c     = new Command;
            c->cwd     = reinterpret_cast<const char*>(reader.read_until('\0'));
            c->command = reinterpret_cast<const char*>(reader.read_until('\0'));
            jobs.back().set_command(c);
        } break;
        case ClientChunkType::ARGUMENT:
            if(jobs.back().has_arg()) {
                jobs.emplace_back();
                jobs.back().set_command(*(jobs.end() - 2));
            }
            jobs.back().set_arg(reinterpret_cast<const char*>(reader.read_until('\0')));
            break;
        }
    }
    return jobs;
}
auto Server::handle_command(const std::string& input) -> bool {
    struct Command {
        const char* command;
        const char* help;
    };
    constexpr Command COMMANDS[] = {
        {"quit", "Quit xserver"},
        {"list", "List connected workers"},
        {"connect", "Connect to remote server"},
        {"help", "Print this help"},
    };
    constexpr auto N_COMMANDS = sizeof(COMMANDS) / sizeof(COMMANDS[0]);
    for(size_t i = 0; i < N_COMMANDS; i += 1) {
        if(input[0] != COMMANDS[i].command[0]) {
            continue;
        }
        switch(i) {
        case 0:
            return false;
        case 1:
            print("Connected workers:");
            for(const auto& w : worker_groups) {
                print("    ", w.get_address() == 0 ? "local" : inet_ntoa({w.get_address()}), " ", w.get_busy(), "/", w.get_workers());
            }
            break;
        case 2: {
            const auto s = input.find(' ');
            if(s == std::string::npos) {
                warn("Invalid address");
                break;
            }
            if(const auto p = add_worker_group(input.substr(s + 1)); p != nullptr) {
                update_epoll_handle_data();
                assign_jobs(p);
            }
            break;
        }
        case 3:
            for(size_t i = 0; i < N_COMMANDS; i += 1) {
                print(COMMANDS[i].command, "   ", COMMANDS[i].help);
            }
            break;
        }
        break;
    }
    return true;
}
auto Server::add_worker_group(const std::string& address) -> WorkerGroup* {
    if(address == "local" || address == "0") {
        if(auto r = open_local_client_socket("\0xrun-local-worker"); r.message != nullptr) {
            warn("Failed to create connection to local server: ", r.message);
            return nullptr;
        } else {
            worker_groups.emplace_back(WorkerGroup(0, r.fd));
            return &worker_groups.back();
        }
    } else {
        const auto addr_opt = parse_str_to_address(address);
        if(!addr_opt.has_value()) {
            warn("Invaid address ", address);
            return nullptr;
        }

        const auto& addr = addr_opt.value();
        if(auto r = open_tcp_client_socket(addr.first, addr.second); r.message != nullptr) {
            warn("Failed to create connection to remote server ", address, ": ", r.message);
            return nullptr;
        } else {
            worker_groups.emplace_back(WorkerGroup(addr.first, r.fd));
            return &worker_groups.back();
        }
    }
}
auto Server::add_epoll_handle(const int fd, const void* const data) -> void {
    auto evset = epoll_event{.events = EPOLLIN, .data = {const_cast<void*>(data)}};
    if(epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &evset) < 0) {
        panic("epoll_ctl() failed: ", errno);
    }
}
auto Server::update_epoll_handle_data() const -> void {
    auto evset = epoll_event{.events = EPOLLIN};
    for(const auto& g : worker_groups) {
        evset.data.ptr = const_cast<WorkerGroup*>(&g);
        epoll_ctl(epfd, EPOLL_CTL_MOD, g.get_fd(), &evset);
    }
}
auto Server::run(const Args& args) -> void {
    // open socket for xrun
    auto xrun_socket = FileDescriptor(-1);
    if(auto r = open_local_server_socket("\0xrun"); r.message != nullptr) {
        panic("xserver already running: ", r.message);
    } else {
        xrun_socket = r.fd;
    }

    // setup epoll
    epfd = FileDescriptor(epoll_create(1));
    if(epfd < 0) {
        panic("epoll_create() failed: ", errno);
    }
    add_epoll_handle(fileno(stdin), nullptr);
    add_epoll_handle(xrun_socket, &xrun_socket);

    // create connection to local worker group
    if(const auto p = add_worker_group("0"); p != nullptr) {
        add_epoll_handle(p->get_fd(), p);
    }

    // create connections to remote worker group
    for(const auto& a : args.remotes) {
        if(const auto p = add_worker_group(a); p != nullptr) {
            add_epoll_handle(p->get_fd(), p);
        }
    }
    if(!worker_groups.empty()) {
        update_epoll_handle_data();
    }

    // main loop
    auto input = std::string();
    auto ev    = epoll_event();
    while(true) {
        if(epoll_wait(epfd, &ev, 1, -1) < 0) {
            panic("epoll_wait() failed: ", errno);
        }
        if(ev.data.ptr == nullptr) {
            // stdin
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
                    if(!handle_command(input)) {
                        break;
                    }
                    input.clear();
                } else {
                    input += buf;
                }
            }
        } else if(ev.data.ptr == &xrun_socket) {
            // xrun
            auto c = Connection::connect(xrun_socket);
            if(!c.has_value()) {
                panic("Failet to accept xrun");
            }
            const auto res = c->get_fd().read_sized();
            if(!res.has_value()) {
                panic("Failed to read packet from xrun");
            }
            const auto received = parse_recieved(*res);
            print("Received ", received.size(), " jobs");
            std::copy(received.begin(), received.end(), std::back_inserter(jobs));
            assign_jobs();
        } else {
            // xworker
            auto& g = *static_cast<WorkerGroup*>(ev.data.ptr);
            if(ev.events & EPOLLHUP || ev.events & EPOLLERR) {
                warn("Connection closed: ", g.get_address() == 0 ? "local" : inet_ntoa({g.get_address()}));
                epoll_ctl(epfd, EPOLL_CTL_DEL, g.get_fd(), NULL);
                worker_groups.erase(worker_groups.begin() + (&g - worker_groups.data()));
                update_epoll_handle_data();
            } else if(ev.events & EPOLLIN) {
                const auto type = g.get_fd().read<WorkerGroupMessage>();
                if(!type.has_value()) {
                    panic("read() failed.");
                }
                switch(*type) {
                case WorkerGroupMessage::DONE:
                    g.decrement_busy();
                    assign_jobs(&g);
                    break;
                case WorkerGroupMessage::ERROR: {
                    const auto r = parse_error_packet(g.get_fd());
                    if(r.exitted) {
                        warn("Command \"", r.command, "\" returned exit code ", static_cast<int>(r.code));
                        warn("=== stdout ===\n", r.out, "\n");
                        warn("=== stderr ===\n", r.err, "\n");
                    } else {
                        warn("Command \"", r.command, "\" terminated by signal ", static_cast<int>(r.code));
                    }
                } break;
                default:
                    panic("Received an invalid message ", static_cast<int>(*type));
                    break;
                }
            }
        }
    }
}
} // namespace xrun
