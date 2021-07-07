#pragma once
#include <condition_variable>
#include <mutex>
#include <optional>

using mutex_lock = std::unique_lock<std::mutex>;

template <class T>
class Channel {
    T                       buffer;
    std::mutex              buffer_mutex;
    std::condition_variable read_cond;
    std::condition_variable write_cond;
    bool                    data_avail = false;

  public:
    void write(T data) {
        mutex_lock lock(buffer_mutex);
        write_cond.wait(lock, [&]() { return !data_avail; });
        buffer     = data;
        data_avail = true;
        read_cond.notify_all();
    }

    T read() {
        mutex_lock lock(buffer_mutex);
        read_cond.wait(lock, [&]() { return data_avail; });
        T item     = buffer;
        data_avail = false;
        write_cond.notify_all();
        return item;
    }

    std::optional<T> try_read() {
        {
            std::lock_guard<std::mutex> lock(buffer_mutex);
            if(!data_avail) {
                return std::nullopt;
            }
        }
        return read();
    }
};
