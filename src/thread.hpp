#pragma once
#include <condition_variable>
#include <functional>
#include <mutex>
#include <optional>

template <typename T>
struct SafeVar {
    mutable std::mutex mutex;
    T                  data;

    auto get_lock() const -> std::lock_guard<std::mutex> {
        return std::lock_guard<std::mutex>(mutex);
    }
    auto store(T src) -> void {
        auto lock = get_lock();
        data      = src;
    }
    auto load() const -> T {
        auto lock = get_lock();
        return data;
    }
    auto operator->() -> T* {
        return &data;
    }
    auto operator*() -> T& {
        return data;
    }
    SafeVar(T src) : data(src) {}
    SafeVar() {}
};

class ConditionalVariable {
  private:
    std::condition_variable condv;
    SafeVar<bool>           waked;

  public:
    void wait() {
        waked.store(false);
        auto lock = std::unique_lock<std::mutex>(waked.mutex);
        condv.wait(lock, [this]() { return waked.data; });
    }
    template <typename D>
    bool wait_for(D duration) {
        waked.store(false);
        auto lock = std::unique_lock<std::mutex>(waked.mutex);
        return condv.wait_for(lock, duration, [this]() { return waked.data; });
    }
    void wakeup() {
        waked.store(true);
        condv.notify_all();
    }
};

template <class T>
class Channel {
    T                       buffer;
    std::mutex              buffer_mutex;
    std::condition_variable read_cond;
    std::condition_variable write_cond;
    bool                    data_avail = false;

  public:
    auto write(T data) -> void {
        auto lock = std::unique_lock<std::mutex>(buffer_mutex);
        write_cond.wait(lock, [&]() { return !data_avail; });
        buffer     = std::move(data);
        data_avail = true;
        read_cond.notify_all();
    }

    auto read() -> T {
        auto lock = std::unique_lock<std::mutex>(buffer_mutex);
        read_cond.wait(lock, [&]() { return data_avail; });
        auto item  = std::move(buffer);
        data_avail = false;
        write_cond.notify_all();
        return item;
    }

    auto try_read() -> std::optional<T> {
        {
            auto lock = std::unique_lock<std::mutex>(buffer_mutex);
            if(!data_avail) {
                return std::nullopt;
            }
        }
        return read();
    }
};
