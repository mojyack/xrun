#pragma once
#include <cstdint>
#include <cstring>
#include <vector>

namespace {
inline auto append_bytes(std::vector<uint8_t>& data, const void* const copy, const size_t len) -> void {
    const auto prev_size = data.size();
    data.resize(prev_size + len);
    std::memcpy(&data[prev_size], copy, len);
}
template <typename T>
auto append_bytes(std::vector<uint8_t>& data, const T copy) -> void {
    append_bytes(data, &copy, sizeof(copy));
}
} // namespace

class ByteReader {
  private:
    const uint8_t* data;
    size_t         pos = 0;
    size_t         lim;

  public:
    template <class T>
    auto read() -> const T* {
        if(pos >= lim) {
            return nullptr;
        }
        pos += sizeof(T);
        return reinterpret_cast<const T*>(data + pos - sizeof(T));
    }
    auto read_until(const char c) -> const uint8_t* {
        const auto cptr = &data[pos];
        for(auto p = pos; p < lim; p += 1) {
            if(data[p] == c) {
                pos = p + 1;
                return cptr;
            }
        }
        return nullptr;
    }
    ByteReader(const std::vector<uint8_t>& data) : data(data.data()), lim(data.size()){};
    ByteReader(const uint8_t* data, const size_t limit) : data(data), lim(limit) {}
};
